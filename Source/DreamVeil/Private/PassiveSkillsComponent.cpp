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

//공격력 증가 증강 효과
void UPassiveSkillsComponent::ExecuteAttackPowerUp()
{
    if (!AttackComponent)
    {
        return;
    }

   AttackComponent->AddAttackPower(ATTACK_POWER_UP_AMOUNT);
}

//방어력 증가 증강 효과
void UPassiveSkillsComponent::ExecuteDefencePowerUp()
{
    if (!DefenceComponent)
    {
        return;
    }

    DefenceComponent->AddDefencePower(DEFENCE_POWER_UP_AMOUNT);
}

//체력 증가 증강 효과
void UPassiveSkillsComponent::ExecuteHealthUp()
{
    if (!HealthComponent)
    {
        return;
    }

    HealthComponent->SetMaxHealth(HealthComponent->GetMaxHealth() + HEALTH_UP_AMOUNT);
}

//광전사 증강 획득
void UPassiveSkillsComponent::ExecuteBerserker()
{
    bBerserker = true;
}

//가시 갑옷 증강 획득
void UPassiveSkillsComponent::ExecuteThornArmor()
{
    bThornArmor = true;
}

//흡혈 증강 획득
void UPassiveSkillsComponent::ExecuteVampire()
{
    bVampire = true;
}

//재생력 증강 획득
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

//일정 간격마다 체력을 회복
void UPassiveSkillsComponent::ProcessRegenerationTick()
{
    if (!HealthComponent)
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
}