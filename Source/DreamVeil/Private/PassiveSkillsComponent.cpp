#include "PassiveSkillsComponent.h"

#include "AttackComponent.h"
#include "DefenceComponent.h"
#include "HealthComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

// 생성자
UPassiveSkillsComponent::UPassiveSkillsComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    AttackComponent = CreateDefaultSubobject<UAttackComponent>(TEXT("AttackComponent"));
    DefenceComponent = CreateDefaultSubobject<UDefenceComponent>(TEXT("DefenceComponent"));
    HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));

    bBerserker = false;
    bVampire = false;
    bThornArmor = false;
    bRegeneration = false;
    bLastFortress = false;
    bBerserkerActivated = false;
    bLastFortressActivated = false;
}

//이번 공격으로 줄 데미지를 계산, 평타 데미지를 만들 때 씀
float UPassiveSkillsComponent::CalculateOutgoingDamage()
{
    if (!AttackComponent)
    {
        return 0.0f;
    }

    return AttackComponent->GetAttackPower();
}

//받은 데미지를 방어력으로 줄여 체력에 적용하고 실제 적용한 데미지를 반환
float UPassiveSkillsComponent::ApplyIncomingDamage(float IncomingDamage)
{
    if (!DefenceComponent || !HealthComponent)
    {
        return 0.0f;
    }

    if (HealthComponent->bIsDead)
    {
        return 0.0f;
    }

    if (IncomingDamage <= 0.0f)
    {
        return 0.0f;
    }

    //방어력으로 깎되 최소 보장치는 남김
    const float FinalDamage = FMath::Max(IncomingDamage - DefenceComponent->GetDefencePower(), MIN_DAMAGE);

    HealthComponent->ApplyDamage(FinalDamage);

    return FinalDamage;
}

//실제 받은 데미지로 가시 갑옷 반사 데미지를 계산 가시 갑옷이 없으면 0
float UPassiveSkillsComponent::CalculateThornReflectDamage(float FinalDamage)
{
    if (!bThornArmor)
    {
        return 0.0f;
    }

    return FinalDamage * THORN_ARMOR_REFLECT_RATIO;
}

//데미지를 입힌 뒤 보유 중인 패시브 효과를 처리 흡혈
void UPassiveSkillsComponent::ProcessOnDamageDealt(float DamageAmount)
{
    if (!bVampire)
    {
        return;
    }

    if (!HealthComponent)
    {
        return;
    }

    if (HealthComponent->bIsDead)
    {
        return;
    }

    HealthComponent->HealHealth(DamageAmount * VAMPIRE_HEAL_RATIO);
}

//공격 컴포넌트 Getter
UAttackComponent* UPassiveSkillsComponent::GetAttackComponent()
{
    return AttackComponent;
}

//수비 컴포넌트 Getter
UDefenceComponent* UPassiveSkillsComponent::GetDefenceComponent()
{
    return DefenceComponent;
}

//체력 컴포넌트 Getter
UHealthComponent* UPassiveSkillsComponent::GetHealthComponent()
{
    return HealthComponent;
}

//공격력 증가 증강을 적용
void UPassiveSkillsComponent::ExecuteAttackPowerUp()
{
    if (!AttackComponent)
    {
        return;
    }

    AttackComponent->AddAttackPower(ATTACK_POWER_UP_AMOUNT);
}

//방어력 증가 증강을 적용
void UPassiveSkillsComponent::ExecuteDefencePowerUp()
{
    if (!DefenceComponent)
    {
        return;
    }

    DefenceComponent->AddDefencePower(DEFENCE_POWER_UP_AMOUNT);
}

//체력 증가 증강을 적용
void UPassiveSkillsComponent::ExecuteHealthUp()
{
    if (!HealthComponent)
    {
        return;
    }

    HealthComponent->SetMaxHealth(HealthComponent->GetMaxHealth() + HEALTH_UP_AMOUNT);
    HealthComponent->HealHealth(HEALTH_UP_AMOUNT);
}

//광전사 증강을 활성화하고 즉시 상태를 반영
void UPassiveSkillsComponent::ExecuteBerserker()
{
    if (bBerserker)
    {
        return;
    }

    bBerserker = true;

    UpdateBerserkerState();
}

//최후의 요새 증강을 활성화하고 즉시 상태를 반영
void UPassiveSkillsComponent::ExecuteLastFortress()
{
    if (bLastFortress)
    {
        return;
    }

    bLastFortress = true;

    UpdateLastFortressState();
}

//가시 갑옷 증강을 활성화
void UPassiveSkillsComponent::ExecuteThornArmor()
{
    if (bThornArmor)
    {
        return;
    }

    bThornArmor = true;
}

//흡혈 증강을 활성화
void UPassiveSkillsComponent::ExecuteVampire()
{
    if (bVampire)
    {
        return;
    }

    bVampire = true;
}

//재생력 증강을 활성화하고 회복 타이머를 시작
void UPassiveSkillsComponent::ExecuteRegeneration()
{
    if (bRegeneration)
    {
        return;
    }

    bRegeneration = true;

    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        RegenerationTimerHandle,
        this,
        &UPassiveSkillsComponent::ProcessRegenerationTick,
        REGENERATION_INTERVAL,
        true
    );
}

//체력 상태에 따라 광전사 효과를 갱신
void UPassiveSkillsComponent::UpdateBerserkerState()
{
    if (!AttackComponent || !HealthComponent)
    {
        return;
    }

    if (!bBerserker)
    {
        return;
    }

    const bool bShouldActivate = HealthComponent->GetHealthPercentage() <= BERSERKER_THRESHOLD;

    if (bShouldActivate && !bBerserkerActivated)
    {
        AttackComponent->MultiplyAttackPower(BERSERKER_MULTIPLIER);
        bBerserkerActivated = true;
        return;
    }

    if (!bShouldActivate && bBerserkerActivated)
    {
        AttackComponent->MultiplyAttackPower(1.0f / BERSERKER_MULTIPLIER);
        bBerserkerActivated = false;
    }
}

//체력 상태에 따라 최후의 요새 효과를 갱신
void UPassiveSkillsComponent::UpdateLastFortressState()
{
    if (!DefenceComponent || !HealthComponent)
    {
        return;
    }

    if (!bLastFortress)
    {
        return;
    }

    const bool bShouldActivate = HealthComponent->GetHealthPercentage() <= LAST_FORTRESS_THRESHOLD;

    if (bShouldActivate && !bLastFortressActivated)
    {
        DefenceComponent->MultiplyDefencePower(LAST_FORTRESS_MULTIPLIER);
        bLastFortressActivated = true;
        return;
    }

    if (!bShouldActivate && bLastFortressActivated)
    {
        DefenceComponent->MultiplyDefencePower(1.0f / LAST_FORTRESS_MULTIPLIER);
        bLastFortressActivated = false;
    }
}

//현재 체력이 바뀔 때 광전사와 최후의 요새 상태를 갱신
void UPassiveSkillsComponent::HandleCurrentHealthChanged(float OldValue, float NewValue)
{
    UpdateBerserkerState();
    UpdateLastFortressState();
}

//일정 간격마다 체력을 회복
void UPassiveSkillsComponent::ProcessRegenerationTick()
{
    if (!HealthComponent)
    {
        return;
    }

    if (HealthComponent->bIsDead)
    {
        return;
    }

    HealthComponent->HealHealth(REGENERATION_HEAL_AMOUNT);
}

//생명주기 함수
void UPassiveSkillsComponent::BeginPlay()
{
    Super::BeginPlay();

    if (!HealthComponent)
    {
        return;
    }

    HealthComponent->OnCurrentHealthChanged.AddDynamic(this, &UPassiveSkillsComponent::HandleCurrentHealthChanged);
}
