#include "FloatingObjectSpawner.h"
#include "FloatingObject.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

AFloatingObjectSpawner::AFloatingObjectSpawner()
{
    PrimaryActorTick.bCanEverTick = false;
    // Box Extent: 각 축 전체 길이의 절반
    SpawnBox = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnBox"));
    SetRootComponent(SpawnBox);
    SpawnBox->SetBoxExtent(FVector(1500.0, 1500.0, 600.0));
    SpawnBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SpawnBox->SetGenerateOverlapEvents(false);
    SpawnBox->SetCanEverAffectNavigation(false);
}
void AFloatingObjectSpawner::SpawnObjects()
{
    // 액터를 생성할 월드
    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return;
    }
    // 생성 가능한 BP를 보관할 임시 목록
    TArray<TSubclassOf<AFloatingObject>> ValidClasses;
    // Candidate: 등록된 클래스 한 개
    for (const TSubclassOf<AFloatingObject>& Candidate : ObjectClasses)
    {
        if (!Candidate.Get() || Candidate->HasAnyClassFlags(CLASS_Abstract))
        {
            continue;
        }
        ValidClasses.Add(Candidate);
    }
    if (ValidClasses.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: assign at least one FloatingObject Blueprint."), *GetName());
        return;
    }
    // 생성 시점의 박스 좌표계와 축별 스케일
    const FTransform MOVEMENT_SPACE = SpawnBox->GetComponentTransform();
    const FVector VOLUME_SCALE = MOVEMENT_SPACE.GetScale3D().GetAbs();
    if (VOLUME_SCALE.GetMin() <= KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: SpawnBox scale cannot be zero."), *GetName());
        return;
    }
    // 경계 여유를 제외한 로컬 생성 범위
    const FVector VOLUME_EXTENT = SpawnBox->GetUnscaledBoxExtent() - FVector(FMath::Max(0.0f, BoundaryMargin));
    if (VOLUME_EXTENT.GetMin() <= 0.0)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s: BoundaryMargin must be smaller than every Box Extent axis."), *GetName());
        return;
    }
    // 음수를 제외한 요청 개수, ObjectIndex: 현재 생성 순서
    const int32 REQUESTED_COUNT = FMath::Max(0, SpawnCount);
    for (int32 ObjectIndex = 0; ObjectIndex < REQUESTED_COUNT; ++ObjectIndex)
    {
        // 무작위로 고른 클래스의 인덱스
        const int32 CLASS_INDEX = FMath::RandRange(0, ValidClasses.Num() - 1);
        // AttemptIndex: 같은 종류의 오브젝트를 다른 위치에 생성하는 시도 순서
        for (int32 AttemptIndex = 0; AttemptIndex < FMath::Max(1, MaxSpawnAttemptsPerObject); ++AttemptIndex)
        {
            // 박스 내부에서 선택한 로컬 생성 위치
            const FVector LOCAL_POINT(
                FMath::FRandRange(-VOLUME_EXTENT.X, VOLUME_EXTENT.X),
                FMath::FRandRange(-VOLUME_EXTENT.Y, VOLUME_EXTENT.Y),
                FMath::FRandRange(-VOLUME_EXTENT.Z, VOLUME_EXTENT.Z));
            // 변환된 월드 생성 위치
            const FVector WORLD_POINT = MOVEMENT_SPACE.TransformPosition(LOCAL_POINT);
            // 옵션에 따라 정한 시작 회전
            const FRotator INITIAL_ROTATION = bRandomInitialRotation
                ? FRotator(FMath::FRandRange(-180.0f, 180.0f),
                    FMath::FRandRange(-180.0f, 180.0f), FMath::FRandRange(-180.0f, 180.0f))
                : FRotator::ZeroRotator;
            // 다른 차단 충돌체와 겹치는 위치는 생성 거절
            FActorSpawnParameters SpawnParameters;
            SpawnParameters.Owner = this;
            SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
            // 이번에 생성한 오브젝트
            AFloatingObject* Object = World->SpawnActor<AFloatingObject>(ValidClasses[CLASS_INDEX], WORLD_POINT, INITIAL_ROTATION, SpawnParameters);
            if (!IsValid(Object))
            {
                continue;
            }
            // 영역만 전달하고 세부 이동 범위 계산은 오브젝트에 위임
            Object->ConfigureMovement(MOVEMENT_SPACE, LOCAL_POINT, VOLUME_EXTENT);
            SpawnedObjects.Add(Object);
            break;
        }
    }
    UE_LOG(LogTemp, Display, TEXT("%s: spawned %d / %d floating objects."), *GetName(), SpawnedObjects.Num(), REQUESTED_COUNT);
}
void AFloatingObjectSpawner::BeginPlay()
{
    Super::BeginPlay();
    SpawnObjects();
}
