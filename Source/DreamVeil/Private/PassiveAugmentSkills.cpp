#include "PassiveAugmentSkills.h"

#include "AugmentTypes.h"
#include "CombatStatsComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

//공격력 증가
void UAttackUpSkill::Apply()
{
    UCombatStatsComponent* Stats = GetStats();

    if (!Stats)
    {
        return;
    }

    Stats->AddAttackPower(ATTACK_POWER_UP_AMOUNT);
}

//방어력 증가
void UDefenceUpSkill::Apply()
{
    UCombatStatsComponent* Stats = GetStats();

    if (!Stats)
    {
        return;
    }

    Stats->AddDefencePower(DEFENCE_POWER_UP_AMOUNT);
}

//체력 증가
void UHealthUpSkill::Apply()
{
    UCombatStatsComponent* Stats = GetStats();

    if (!Stats)
    {
        return;
    }

    //SetMaxHealth는 현재 체력 변화 이벤트를 보내지 않아서 체력 비율이 바뀌어도 광전사 최후의 요새가 다시 판정하지 않음
    //바로 뒤 Heal이 현재 체력을 바꾸면서 이벤트를 보내 다시 판정하게 됨 Heal을 빼거나 순서를 바꾸지 말 것
    Stats->SetMaxHealth(Stats->GetMaxHealth() + HEALTH_UP_AMOUNT);
    Stats->Heal(HEALTH_UP_AMOUNT);
}

//광전사 얻은 즉시 현재 체력으로 판단
void UBerserkerSkill::Apply()
{
    UpdateState();
}

//광전사 체력이 바뀔 때마다 다시 판단
void UBerserkerSkill::OnHealthChanged(float OldValue, float NewValue)
{
    UpdateState();
}

//광전사 체력 비율을 보고 공격력 배율을 켜거나 끔
void UBerserkerSkill::UpdateState()
{
    UCombatStatsComponent* Stats = GetStats();

    if (!Stats)
    {
        return;
    }

    const bool bShouldActivate = Stats->GetHealthPercentage() <= BERSERKER_THRESHOLD;

    if (bShouldActivate && !bActivated)
    {
        Stats->MultiplyAttackPower(BERSERKER_MULTIPLIER);
        bActivated = true;
        return;
    }

    if (!bShouldActivate && bActivated)
    {
        Stats->MultiplyAttackPower(1.0f / BERSERKER_MULTIPLIER);
        bActivated = false;
    }
}

//최후의 요새 얻은 즉시 현재 체력으로 판단
void ULastFortressSkill::Apply()
{
    UpdateState();
}

//최후의 요새 체력이 바뀔 때마다 다시 판단
void ULastFortressSkill::OnHealthChanged(float OldValue, float NewValue)
{
    UpdateState();
}

//최후의 요새 체력 비율을 보고 방어력 배율을 켜거나 끔
void ULastFortressSkill::UpdateState()
{
    UCombatStatsComponent* Stats = GetStats();

    if (!Stats)
    {
        return;
    }

    const bool bShouldActivate = Stats->GetHealthPercentage() <= LAST_FORTRESS_THRESHOLD;

    if (bShouldActivate && !bActivated)
    {
        Stats->MultiplyDefencePower(LAST_FORTRESS_MULTIPLIER);
        bActivated = true;
        return;
    }

    if (!bShouldActivate && bActivated)
    {
        Stats->MultiplyDefencePower(1.0f / LAST_FORTRESS_MULTIPLIER);
        bActivated = false;
    }
}

//가시 갑옷 받은 데미지의 일정 비율을 반사
float UThornArmorSkill::CalculateReflectDamage(float FinalDamage)
{
    return FinalDamage * THORN_ARMOR_REFLECT_RATIO;
}

//흡혈 입힌 데미지의 일정 비율만큼 회복 죽은 상태면 Heal 안에서 무시됨
void UVampireSkill::OnDamageDealt(float FinalDamage)
{
    UCombatStatsComponent* Stats = GetStats();

    if (!Stats)
    {
        return;
    }

    Stats->Heal(FinalDamage * VAMPIRE_HEAL_RATIO);
}

//재생력 회복 타이머 시작 이미 돌고 있으면 무시
void URegenerationSkill::Apply()
{
    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    if (World->GetTimerManager().IsTimerActive(RegenerationTimerHandle))
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        RegenerationTimerHandle,
        this,
        &URegenerationSkill::ProcessRegenerationTick,
        REGENERATION_INTERVAL,
        true
    );
}

//재생력 타이머 정리
void URegenerationSkill::Deactivate()
{
    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    World->GetTimerManager().ClearTimer(RegenerationTimerHandle);
}

//재생력 일정 간격마다 체력 회복 죽은 상태면 Heal 안에서 무시됨
void URegenerationSkill::ProcessRegenerationTick()
{
    UCombatStatsComponent* Stats = GetStats();

    if (!Stats)
    {
        return;
    }

    Stats->Heal(REGENERATION_HEAL_AMOUNT);
}
