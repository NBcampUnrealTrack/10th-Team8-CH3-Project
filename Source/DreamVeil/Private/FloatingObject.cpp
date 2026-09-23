#include "FloatingObject.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"

AFloatingObject::AFloatingObject()
{
    PrimaryActorTick.bCanEverTick = true;
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
    MoveToTarget(DeltaTime);
    RotateObject(DeltaTime);
}
