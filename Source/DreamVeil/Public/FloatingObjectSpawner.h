#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FloatingObjectSpawner.generated.h"

class AFloatingObject;
class UBoxComponent;

// 지정한 영역에 정해진 개수의 오브젝트 생성
UCLASS()
class AFloatingObjectSpawner : public AActor
{
    GENERATED_BODY()

private:
    // Details에서 생성 시 무작위 회전 적용 여부 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner", meta = (AllowPrivateAccess = "true"))
    bool bRandomInitialRotation = true;
    // Details에서 오브젝트 한 개당 최대 생성 시도 횟수 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner", meta = (AllowPrivateAccess = "true", ClampMin = "1"))
    int32 MaxSpawnAttemptsPerObject = 30;
    // Details에서 전체 생성 개수 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner", meta = (AllowPrivateAccess = "true", ClampMin = "0"))
    int32 SpawnCount = 30;
    // Details에서 경계와 피벗 사이의 여유 설정, 메시 크기는 별도 고려
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    float BoundaryMargin = 100.0f;
    // Details에서 메시가 지정된 오브젝트 BP 등록
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawner", meta = (AllowPrivateAccess = "true"))
    TArray<TSubclassOf<AFloatingObject>> ObjectClasses;
    // 생성 결과 확인용 배열, 외부 변경 제한
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Spawner", meta = (AllowPrivateAccess = "true"))
    TArray<TObjectPtr<AFloatingObject>> SpawnedObjects;
    // 컴포넌트 교체 제한, Details에서 Box Extent 수정
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spawner", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBoxComponent> SpawnBox;
    // 설정 검증 및 무작위 위치에 오브젝트 생성
    void SpawnObjects();
    // 게임 시작 시 한 번 생성
    virtual void BeginPlay() override;

public:
    // 충돌 없는 생성 영역 구성
    AFloatingObjectSpawner();
};
