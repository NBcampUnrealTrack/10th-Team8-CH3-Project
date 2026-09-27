#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "SingleAnimationPlayData.h"
#include "TimerManager.h"
#include "MonsterSkill.generated.h"

class UAnimSequence;
class UDecalComponent;
class AMonsterProjectile;

// 새 패턴은 여기에 추가하고 컴포넌트의 실행 분기에 구현한다.
UENUM(BlueprintType)
enum class EMonsterSkillType : uint8
{
    Charge,
    // 부채꼴로 투사체 여러 발을 동시에 발사한다.
    FanShot
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

    // ---------------- 부채꼴 투사체 (FanShot) ----------------

    //발사할 투사체. 비워두면 몬스터에 설정된 RangedProjectile을 사용한다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Skill|FanShot")
    TSubclassOf<AMonsterProjectile> FanShotProjectile;

    //투사체 한 발당 피해량. 돌진과 마찬가지로 방어력 등은 기존 피해 처리에서 반영된다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Skill|FanShot", meta = (ClampMin = "0"))
    float FanShotDamage = 15.0f;

    //한 번에 발사하는 투사체 수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Skill|FanShot", meta = (ClampMin = "1", ClampMax = "32"))
    int32 FanShotCount = 5;

    //부채꼴 전체 각도(도). 60이면 정면 기준 좌우 30도씩 퍼진다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Skill|FanShot", meta = (ClampMin = "0", ClampMax = "360"))
    float FanShotAngle = 60.0f;

    //플레이어가 이 거리 안에 있을 때만 발동한다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Skill|FanShot", meta = (ClampMin = "0"))
    float FanShotTriggerDistance = 1500.0f;

    //경고 표시 후 실제 발사까지의 준비 시간(초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Skill|FanShot", meta = (ClampMin = "0"))
    float FanShotReadySeconds = 0.8f;

    //발사 후 다시 움직이기까지의 경직 시간(초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Skill|FanShot", meta = (ClampMin = "0"))
    float FanShotRecoverySeconds = 0.3f;

    //경고 표시 길이와 폭(cm). 투사체 경로마다 한 줄씩 그린다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Skill|FanShot", meta = (ClampMin = "0"))
    float FanShotWarningLength = 1500.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Skill|FanShot", meta = (ClampMin = "0"))
    float FanShotWarningWidth = 60.0f;

    //준비 동작과 발사 동작. 비워두면 원래 AnimBP를 그대로 쓴다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Skill|FanShot")
    TObjectPtr<UAnimSequence> FanShotReadyAnimation;

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
    void CheckSkillRange();
    bool TryUseCharge();
    void StartCharge();
    void CheckChargeHits(const FVector& Start, const FVector& End);
    void CleanupCharge();
    void PlayChargeAnimation(UAnimSequence* Animation);

    //부채꼴 투사체
    bool TryUseFanShot();
    void FireFanShot();
    void CleanupFanShot();
    void ShowFanShotWarning(const FVector& Origin);
    void HideFanShotWarning();
    TArray<FVector> GetFanShotDirections() const;

    FTimerHandle FanShotReadyTimer;
    FTimerHandle FanShotRecoveryTimer;
    FVector FanShotDirection = FVector::ZeroVector;
    bool bFanShotPrepared = false;
    //경로마다 하나씩 만드는 임시 경고 데칼
    UPROPERTY(Transient)
    TArray<TObjectPtr<UDecalComponent>> FanShotWarningDecals;

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
