#include "FloatingObject.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"

// 화면에서 사라진 뒤 이 시간이 지나면 갱신을 멈춤
// 0으로 두지 않는 이유 카메라를 홱 돌릴 때 한 프레임만 안 보여도 바로 멈추면 움직임이 끊겨 보임
static const float RENDER_IDLE_TOLERANCE = 0.5f;

// 갱신 간격 초당 20번 장식용이라 매 프레임까지 갱신할 이유가 없음
// 이동과 회전이 DeltaTime을 곱해서 계산하므로 간격을 늘려도 속도는 그대로임
static const float FLOATING_TICK_INTERVAL = 0.05f;

AFloatingObject::AFloatingObject()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = FLOATING_TICK_INTERVAL;
    // 이동 경로를 검사할 구형 루트, 반경은 BP에서 메시 크기에 맞게 조절
    CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
    SetRootComponent(CollisionSphere);
    CollisionSphere->InitSphereRadius(150.0f);
    CollisionSphere->SetMobility(EComponentMobility::Movable);
    CollisionSphere->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CollisionSphere->SetGenerateOverlapEvents(false);
    CollisionSphere->SetCanEverAffectNavigation(false);
    // 외형 메시를 충돌체에 부착하고 중복 충돌 비활성화
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(CollisionSphere);
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetSimulatePhysics(false);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetGenerateOverlapEvents(false);
    Mesh->SetCanEverAffectNavigation(false);
}

void AFloatingObject::ChooseTarget()
{
    // 로컬 이동 범위에서 무작위 목표점 선택
    const FVector LOCAL_TARGET(
        FMath::FRandRange(LocalMin.X, LocalMax.X),
        FMath::FRandRange(LocalMin.Y, LocalMax.Y),
        FMath::FRandRange(LocalMin.Z, LocalMax.Z));
    TargetLocation = MovementSpace.TransformPosition(LOCAL_TARGET);
}
void AFloatingObject::ConfigureMovement(const FTransform& InSpace,
    const FVector& InSpawnPoint, const FVector& InVolumeExtent)
{
    // 오브젝트 자신의 설정을 이용해 이동 범위 계산
    const FVector MOVEMENT_RANGE = MovementHalfExtent.GetAbs();
    const FVector VOLUME_EXTENT = InVolumeExtent.GetAbs();
    MovementSpace = InSpace;
    // 범위 밖 생성 위치는 가장 가까운 내부 지점으로 제한
    const FVector SPAWN_POINT = InSpawnPoint.ComponentMax(-VOLUME_EXTENT).ComponentMin(VOLUME_EXTENT);
    LocalMin = (SPAWN_POINT - MOVEMENT_RANGE).ComponentMax(-VOLUME_EXTENT);
    LocalMax = (SPAWN_POINT + MOVEMENT_RANGE).ComponentMin(VOLUME_EXTENT);
    ChooseTarget();
}
void AFloatingObject::MoveToTarget(float DeltaTime)
{
    if (!bEnableMovement || MoveSpeed <= 0.0f)
    {
        return;
    }
    // 프레임 시간을 반영한 다음 위치
    const FVector NEXT_LOCATION = FMath::VInterpConstantTo(GetActorLocation(), TargetLocation, DeltaTime, MoveSpeed);
    // 루트 충돌체의 이동 경로 검사 결과
    FHitResult HitResult;
    SetActorLocation(NEXT_LOCATION, true, &HitResult);
    // 장애물에 막히면 새 목표점 선택
    if (HitResult.bBlockingHit)
    {
        ChooseTarget();
        return;
    }
    if (!NEXT_LOCATION.Equals(TargetLocation, 1.0f))
    {
        return;
    }
    ChooseTarget();
}
void AFloatingObject::RotateObject(float DeltaTime)
{
    if (!bEnableRotation)
    {
        return;
    }
    // 초당 회전량을 이번 프레임의 회전량으로 변환
    AddActorLocalRotation(FRotator(
        RotationSpeed.Pitch * DeltaTime,
        RotationSpeed.Yaw * DeltaTime,
        RotationSpeed.Roll * DeltaTime));
}
void AFloatingObject::BeginPlay()
{
    Super::BeginPlay();
    // 음수 제외 및 최소/최대 이동 속도 정리
    const float MIN_MOVE_SPEED = FMath::Max(0.0f, FMath::Min(static_cast<float>(MoveSpeedRange.X), static_cast<float>(MoveSpeedRange.Y)));
    const float MAX_MOVE_SPEED = FMath::Max(MIN_MOVE_SPEED, FMath::Max(static_cast<float>(MoveSpeedRange.X), static_cast<float>(MoveSpeedRange.Y)));
    MoveSpeed = FMath::FRandRange(MIN_MOVE_SPEED, MAX_MOVE_SPEED);
    // 축별 양방향 회전 속도의 한계
    const FVector MAX_ROTATION_SPEED = MaxRotationSpeed.GetAbs();
    RotationSpeed = FRotator(
        FMath::FRandRange(-MAX_ROTATION_SPEED.Y, MAX_ROTATION_SPEED.Y),
        FMath::FRandRange(-MAX_ROTATION_SPEED.Z, MAX_ROTATION_SPEED.Z),
        FMath::FRandRange(-MAX_ROTATION_SPEED.X, MAX_ROTATION_SPEED.X));
    // 직접 배치한 액터의 시작 위치 주변으로 이동 범위 설정
    ConfigureMovement(FTransform(FQuat::Identity, GetActorLocation()), FVector::ZeroVector, MovementHalfExtent.GetAbs());
}
void AFloatingObject::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    // 화면에 안 보이는 동안은 갱신하지 않음
    // 이동할 때마다 SetActorLocation의 경로 검사(구체 스윕)가 들어가는데 장식이 맵에 수십 개 깔리면
    // 보이지도 않는 충돌 검사가 그 수만큼 계속 돌게 됨 몬스터가 많은 판에서는 이게 그대로 쌓임
    // 멈췄다가 다시 보일 때 그 자리에서 이어서 움직이므로 느리게 떠다니는 장식에서는 티가 나지 않음
    if (!WasRecentlyRendered(RENDER_IDLE_TOLERANCE))
    {
        return;
    }
    MoveToTarget(DeltaTime);
    RotateObject(DeltaTime);
}
