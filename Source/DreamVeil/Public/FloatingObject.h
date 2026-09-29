#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FloatingObject.generated.h"

class USphereComponent;
class UStaticMeshComponent;

// 메시의 부유 이동과 회전 담당
UCLASS()
class AFloatingObject : public AActor
{
    GENERATED_BODY()

private:
    // 실행 중 이동 속도 (cm/s)
    float MoveSpeed = 0.0f;
    // 실행 중 축별 회전 속도 (도/s)
    FRotator RotationSpeed = FRotator::ZeroRotator;
    // 이동 범위의 기준 좌표계
    FTransform MovementSpace = FTransform::Identity;
    // 로컬 좌표 기준 최대 이동 경계
    FVector LocalMax = FVector::ZeroVector;
    // 로컬 좌표 기준 최소 이동 경계
    FVector LocalMin = FVector::ZeroVector;
    // 현재 이동 중인 월드 목표점
    FVector TargetLocation = FVector::ZeroVector;
    // Details에서 이동 여부 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Object|Movement", meta = (AllowPrivateAccess = "true"))
    bool bEnableMovement = true;
    // Details에서 회전 여부 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Object|Rotation", meta = (AllowPrivateAccess = "true"))
    bool bEnableRotation = true;
    // Details에서 축별 최대 회전 속도 설정 (X=Roll, Y=Pitch, Z=Yaw)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Object|Rotation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    FVector MaxRotationSpeed = FVector(8.0, 12.0, 18.0);
    // Details에서 생성 위치 주변의 축별 이동 거리 설정
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Object|Movement", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
    FVector MovementHalfExtent = FVector(300.0, 300.0, 150.0);
    // Details에서 최소/최대 이동 속도 설정 (월드 cm/s)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Object|Movement", meta = (AllowPrivateAccess = "true"))
    FVector2D MoveSpeedRange = FVector2D(30.0, 80.0);
    // 회전하는 메시 전체를 감쌀 루트 충돌체, Details에서 Sphere Radius 설정
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Object|Collision", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USphereComponent> CollisionSphere;
    // 컴포넌트 교체 제한, Details에서 내부 Static Mesh 지정
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Object", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> Mesh;
    // 허용된 이동 범위에서 다음 목표점 선택
    void ChooseTarget();
    // DeltaTime을 반영해 목표점 방향으로 이동
    void MoveToTarget(float DeltaTime);
    // DeltaTime을 반영해 개체별 속도로 회전
    void RotateObject(float DeltaTime);
    // 게임 시작 시 이동 범위와 무작위 속도 초기화
    virtual void BeginPlay() override;
    // 매 프레임 이동과 회전 갱신
    virtual void Tick(float DeltaTime) override;

public:
    // 메시 생성 및 기본 동작 설정
    AFloatingObject();
    // 전달받은 생성 영역 안에서 자신의 이동 범위 계산
    void ConfigureMovement(const FTransform& InSpace, const FVector& InSpawnPoint, const FVector& InVolumeExtent);
};