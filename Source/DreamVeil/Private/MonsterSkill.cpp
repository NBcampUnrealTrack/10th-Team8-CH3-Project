#include "MonsterSkill.h"

#include "MonsterBase.h"
#include "CombatStatsComponent.h"
#include "Engine/World.h"

UMonsterSkill::UMonsterSkill()
{
    // 시간 비교로 쿨타임을 확인하므로 Tick이나 반복 타이머는 필요 없다.
    PrimaryComponentTick.bCanEverTick = false;
}

const FMonsterSkillSettings* UMonsterSkill::FindSettings(EMonsterSkillType Skill) const
{
    return Skills.FindByPredicate([Skill](const FMonsterSkillSettings& Settings)
    {
        return Settings.Skill == Skill;
    });
}

void UMonsterSkill::SetProgression(int32 Phase, int32 Level)
{
    CurrentPhase = FMath::Max(1, Phase);
    CurrentLevel = FMath::Max(1, Level);
}

float UMonsterSkill::GetCooldownRemaining(EMonsterSkillType Skill) const
{
    const double* NextUseTime = NextUseTimes.Find(Skill);
    return NextUseTime && GetWorld()
        ? static_cast<float>(FMath::Max(0.0, *NextUseTime - GetWorld()->GetTimeSeconds()))
        : 0.0f;
}

bool UMonsterSkill::CanUseSkill(EMonsterSkillType Skill) const
{
    const AMonsterBase* Monster = Cast<AMonsterBase>(GetOwner());
    const FMonsterSkillSettings* Settings = FindSettings(Skill);
    return GetWorld() && IsValid(Monster) && Settings
        && !bIsUsingSkill && !Monster->IsAttacking()
        && Monster->MonsterCombatStats && !Monster->MonsterCombatStats->IsDead()
        && CurrentPhase >= Settings->RequiredPhase
        && CurrentLevel >= Settings->RequiredLevel
        && GetCooldownRemaining(Skill) <= 0.0f;
}

bool UMonsterSkill::BeginSkill(EMonsterSkillType Skill)
{
    if (!CanUseSkill(Skill)) return false;

    ActiveSkill = Skill;
    ActiveCooldown = FMath::Max(0.0f, FindSettings(Skill)->Cooldown);
    bIsUsingSkill = true;
    return true;
}

void UMonsterSkill::FinishSkill()
{
    if (!bIsUsingSkill) return;

    if (GetWorld())
    {
        NextUseTimes.Add(ActiveSkill, GetWorld()->GetTimeSeconds() + ActiveCooldown);
    }
    bIsUsingSkill = false;
}

void UMonsterSkill::CancelSkill()
{
    if (!bIsUsingSkill) return;

    if (AMonsterBase* Monster = Cast<AMonsterBase>(GetOwner()))
    {
        Monster->HideAttackWarning();
    }
    FinishSkill();
}
