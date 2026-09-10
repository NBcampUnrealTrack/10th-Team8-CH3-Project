#include "PassiveSkillsComponent.h"

#include "AttackComponent.h"
#include "DefenceComponent.h"
#include "HealthComponent.h"

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
    bBerserkerActivated = false;
}

//증강 효과 함수들을 테이블에 등록
void UPassiveSkillsComponent::RegisterPassiveAugmentFunctions()
{
    PassiveAugmentMap.Add(EAugmentID::AttackUp, [this]() { ExecuteAttackPowerUp(); });
    PassiveAugmentMap.Add(EAugmentID::DefenceUp, [this]() { ExecuteDefencePowerUp(); });
    PassiveAugmentMap.Add(EAugmentID::HealthUp, [this]() { ExecuteHealthUp(); });
    PassiveAugmentMap.Add(EAugmentID::Berserker, [this]() { ExecuteBerserker(); });
    PassiveAugmentMap.Add(EAugmentID::ThornArmor, [this]() { ExecuteThornArmor(); });
    PassiveAugmentMap.Add(EAugmentID::Vampire, [this]() { ExecuteVampire(); });
    PassiveAugmentMap.Add(EAugmentID::Regeneration, [this]() { ExecuteRegeneration(); });
}

//번호로 패시브 증강 효과를 실행
void UPassiveSkillsComponent::ExecutePassiveAugment(EAugmentID AugmentID)
{
    TFunction<void()>* FoundFunction = PassiveAugmentMap.Find(AugmentID);

    if (!FoundFunction)
    {
        return;
    }

        (*FoundFunction)();
    }

//이번 공격으로 줄 데미지를 계산
float UPassiveSkillsComponent::CalculateOutgoingDamage()
{
    if (!AttackComponent)
    {
        return 0.0f;
    }

    return AttackComponent->GetAttackPower();
}

//받은 데미지를 방어력으로 줄여 체력에 적용하고 반사 데미지를 반환
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

    const float FinalDamage = FMath::Max(IncomingDamage - DefenceComponent->GetDefencePower(), MIN_DAMAGE);

    HealthComponent->ApplyDamage(FinalDamage);

    if (!bThornArmor)
    {
        return 0.0f;
    }

    return FinalDamage * THORN_ARMOR_REFLECT_RATIO;
}

//데미지를 입힌 뒤 보유 중인 패시브 효과를 처리
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

    HealthComponent->HealHealth(DamageAmount * VAMPIRE_HEAL_RATIO);
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

//현재 체력이 바뀔 때 광전사 상태를 갱신
void UPassiveSkillsComponent::HandleCurrentHealthChanged(float OldValue, float NewValue)
{
    UpdateBerserkerState();
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

    RegisterPassiveAugmentFunctions();

    if (!HealthComponent)
    {
        return;
    }

    HealthComponent->OnCurrentHealthChanged.AddDynamic(this, &UPassiveSkillsComponent::HandleCurrentHealthChanged);
}