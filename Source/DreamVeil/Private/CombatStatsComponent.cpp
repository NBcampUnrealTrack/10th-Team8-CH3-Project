#include "CombatStatsComponent.h"

#include "AugmentTypes.h"

//생성자
UCombatStatsComponent::UCombatStatsComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

//스탯 전체 복사본
FCombatStats UCombatStatsComponent::GetStats() const
{
    return Stats;
}

//최종 공격력
float UCombatStatsComponent::GetAttackPower() const
{
    return Stats.GetAttackPower();
}

//최종 방어력
float UCombatStatsComponent::GetDefencePower() const
{
    return Stats.GetDefencePower();
}

//현재 체력
float UCombatStatsComponent::GetCurrentHealth() const
{
    return Stats.CurrentHealth;
}

//최대 체력
float UCombatStatsComponent::GetMaxHealth() const
{
    return Stats.MaxHealth;
}

//체력 비율 0~1
float UCombatStatsComponent::GetHealthPercentage() const
{
    return Stats.GetHealthPercentage();
}

//죽었는지 여부
bool UCombatStatsComponent::IsDead() const
{
    return Stats.bIsDead;
}

//공방체 기본값을 넣고 죽음 상태를 풀고 체력을 가득 채움
void UCombatStatsComponent::InitStats(float NewMaxHealth, float NewDefencePower, float NewAttackPower)
{
    //최종값은 매번 계산하므로 기본값만 바꾸면 바로 반영됨
    Stats.BaseDefencePower = NewDefencePower;
    Stats.BaseAttackPower = NewAttackPower;

    SetMaxHealth(NewMaxHealth);

    ResetHealth();
}

//추가 공격력을 더함
void UCombatStatsComponent::AddAttackPower(float Amount)
{
    Stats.AdditionalAttackPower += Amount;
}

//공격력 배율을 곱함
void UCombatStatsComponent::MultiplyAttackPower(float Multiplier)
{
    Stats.AttackMultiplier *= Multiplier;
}

//추가 방어력을 더함
void UCombatStatsComponent::AddDefencePower(float Amount)
{
    Stats.AdditionalDefencePower += Amount;
}

//방어력 배율을 곱함
void UCombatStatsComponent::MultiplyDefencePower(float Multiplier)
{
    Stats.DefenceMultiplier *= Multiplier;
}

//최대 체력을 바꿈 현재 체력이 더 크면 최대 체력까지 줄임
void UCombatStatsComponent::SetMaxHealth(float NewMaxHealth)
{
    const float OldMaxHealth = Stats.MaxHealth;

    Stats.MaxHealth = FMath::Max(NewMaxHealth, 1.0f);

    if (OldMaxHealth != Stats.MaxHealth)
    {
        OnMaxHealthChanged.Broadcast(OldMaxHealth, Stats.MaxHealth);
    }

    if (Stats.CurrentHealth > Stats.MaxHealth)
    {
        SetCurrentHealth(Stats.MaxHealth);
    }
}

//현재 체력을 바꿈 0이면 사망 0보다 크면 살아 있는 상태로 되돌림
void UCombatStatsComponent::SetCurrentHealth(float NewCurrentHealth)
{
    const float OldCurrentHealth = Stats.CurrentHealth;
    const bool bWasDead = Stats.bIsDead;

    Stats.CurrentHealth = FMath::Clamp(NewCurrentHealth, 0.0f, Stats.MaxHealth);

    if (OldCurrentHealth == Stats.CurrentHealth)
    {
        return;
    }

    //방송 전에 죽음 상태부터 맞춰둠 받는 쪽이 체력 0인데 살아 있는 상태를 보지 않도록
    Stats.bIsDead = Stats.CurrentHealth <= 0.0f;

    OnCurrentHealthChanged.Broadcast(OldCurrentHealth, Stats.CurrentHealth);

    if (Stats.bIsDead && !bWasDead)
    {
        OnDead.Broadcast();
    }
}

//체력 회복 죽은 상태면 무시
void UCombatStatsComponent::Heal(float HealAmount)
{
    if (Stats.bIsDead)
    {
        return;
    }

    SetCurrentHealth(Stats.CurrentHealth + HealAmount);
}

//죽음 상태를 풀고 체력을 가득 채움
void UCombatStatsComponent::ResetHealth()
{
    Stats.bIsDead = false;

    SetCurrentHealth(Stats.MaxHealth);
}

//이번 공격으로 줄 데미지
float UCombatStatsComponent::CalculateOutgoingDamage() const
{
    return Stats.GetAttackPower();
}

//받은 데미지를 방어력으로 줄여 체력에 적용하고 적용한 데미지를 반환
float UCombatStatsComponent::ApplyIncomingDamage(float IncomingDamage, bool bIgnoreDefence)
{
    if (Stats.bIsDead)
    {
        return 0.0f;
    }

    if (IncomingDamage <= 0.0f)
    {
        return 0.0f;
    }

    const float DefencePower = bIgnoreDefence ? 0.0f : Stats.GetDefencePower();

    //방어력으로 깎되 최소 보장치는 남김 남은 체력보다 커도 자르지 않음(오버킬 허용)
    const float FinalDamage = FMath::Max(IncomingDamage - DefencePower, MIN_DAMAGE);

    SetCurrentHealth(Stats.CurrentHealth - FinalDamage);

    return FinalDamage;
}

//생명주기 함수
void UCombatStatsComponent::BeginPlay()
{
    Super::BeginPlay();

    //시작할 때 체력을 가득 채움 이벤트는 보내지 않으므로 UI는 Getter로 시작 값을 읽을 것
    Stats.CurrentHealth = Stats.MaxHealth;
    Stats.bIsDead = false;
}
