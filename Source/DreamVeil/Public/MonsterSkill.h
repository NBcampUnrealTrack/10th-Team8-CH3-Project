#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "SingleAnimationPlayData.h"
#include "TimerManager.h"
#include "MonsterSkill.generated.h"

class UAnimSequence;

// 새 패턴은 여기에 추가하고 컴포넌트의 실행 분기에 구현한다.
UENUM(BlueprintType)
enum class EMonsterSkillType : uint8
{
    Charge
};

USTRUCT(BlueprintType)
struct FMonsterSkillSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EMonsterSkillType Skill = EMonsterSkillType::Charge;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
    float Cooldown = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
    int32 RequiredPhase = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
    int32 RequiredLevel = 1;

};

UCLASS(ClassGroup = (Monster), meta = (BlueprintSpawnableComponent))
class DREAMVEIL_API UMonsterSkill : public UActorComponent
{
    GENERATED_BODY()

public:
    UMonsterSkill();

    // 빈 목록이면 특수공격 없음. 엘리트는 하나, 보스는 전체 패턴을 등록한다.
    // 중복 Skill은 등록하지 않는다. 패턴 선택과 실제 공격 동작은 추후 연결한다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Skill")
    TArray<FMonsterSkillSettings> Skills;

    UFUNCTION(BlueprintCallable, Category = "Monster|Skill")
    void SetProgression(int32 Phase, int32 Level);

    UFUNCTION(BlueprintPure, Category = "Monster|Skill")
    bool CanUseSkill(EMonsterSkillType Skill) const;

    UFUNCTION(BlueprintPure, Category = "Monster|Skill")
    float GetCooldownRemaining(EMonsterSkillType Skill) const;

    // 사용 가능 여부를 검사하고 실행 중 상태만 설정한다. 실제 공격은 아직 구현하지 않는다.
    UFUNCTION(BlueprintCallable, Category = "Monster|Skill")
    bool BeginSkill(EMonsterSkillType Skill);

    // 공격 종료 시 호출. 종료/취소 시점부터 해당 스킬의 쿨타임을 시작한다.
    UFUNCTION(BlueprintCallable, Category = "Monster|Skill")
    void FinishSkill();

    UFUNCTION(BlueprintCallable, Category = "Monster|Skill")
    void CancelSkill();

    UFUNCTION(BlueprintPure, Category = "Monster|Skill")
    bool IsUsingSkill() const { return bIsUsingSkill; }

    //기존 BeginSkill은 상태만 설정한다. 실제 패턴 실행은 이 진입점에서 연결한다.
    UFUNCTION(BlueprintCallable, Category = "Monster|Skill")
    bool TryUseSkill(EMonsterSkillType Skill);

    //플레이어에게 적용할 기본 피해량이며 기존 피해 처리에서 방어력 등이 반영된다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Skill|Charge", meta = (ClampMin = "0"))
    float ChargeDamage = 30.0f;

    //아직 모션이 없으면 비워 둔다. 준비 중에는 이 제자리 걷기를 반복 재생한다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Skill|Charge")
    TObjectPtr<UAnimSequence> ChargeReadyAnimation;

    //돌진 중 반복 재생할 달리기 모션. 이동 거리는 애니메이션이 아니라 코드가 결정한다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Skill|Charge")
    TObjectPtr<UAnimSequence> ChargeRunAnimation;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Skill", meta = (ClampMin = "1"))
    int32 CurrentPhase = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Skill", meta = (ClampMin = "1"))
    int32 CurrentLevel = 1;

private:
    //돌진의 전체 가로 폭(cm). 생성자에서 설정하고 데칼과 공격 판정이 함께 사용한다.
    float ChargeWidth;

    //엘리트는 짧은 간격으로 거리와 쿨타임을 확인하고, 보스는 기존 선택 타이머를 사용한다.
    void CheckChargeRange();
    void StartCharge();
    void CheckChargeHits(const FVector& Start, const FVector& End);
    void CleanupCharge();
    void PlayChargeAnimation(UAnimSequence* Animation);

    FTimerHandle ChargeCheckTimer;
    FTimerHandle ChargeReadyTimer;
    FVector ChargeDirection = FVector::ZeroVector;
    FVector ChargeStart = FVector::ZeroVector;
    FVector PreviousChargeLocation = FVector::ZeroVector;
    double ChargeEndTime = 0.0;
    bool bChargePrepared = false;
    bool bCharging = false;
    bool bSavedAvoidance = false;
    //액터가 파괴돼도 참조를 붙잡지 않으면서 한 번의 돌진에 중복 적중을 막는다.
    TSet<TWeakObjectPtr<AActor>> ChargeHitActors;
    FCollisionResponseContainer SavedCapsuleResponses;
    bool bAnimationOverridden = false;
    EAnimationMode::Type SavedAnimationMode = EAnimationMode::AnimationBlueprint;
    UPROPERTY(Transient)
    FSingleAnimationPlayData SavedAnimationData;

    const FMonsterSkillSettings* FindSettings(EMonsterSkillType Skill) const;
    TMap<EMonsterSkillType, double> NextUseTimes;
    bool bIsUsingSkill = false;
    EMonsterSkillType ActiveSkill = EMonsterSkillType::Charge;
    float ActiveCooldown = 0.0f;
};
