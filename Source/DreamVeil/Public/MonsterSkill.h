#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MonsterSkill.generated.h"

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

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Skill", meta = (ClampMin = "1"))
    int32 CurrentPhase = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Skill", meta = (ClampMin = "1"))
    int32 CurrentLevel = 1;

private:
    const FMonsterSkillSettings* FindSettings(EMonsterSkillType Skill) const;
    TMap<EMonsterSkillType, double> NextUseTimes;
    bool bIsUsingSkill = false;
    EMonsterSkillType ActiveSkill = EMonsterSkillType::Charge;
    float ActiveCooldown = 0.0f;
};
