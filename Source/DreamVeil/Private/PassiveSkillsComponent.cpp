#include "PassiveSkillsComponent.h"

#include "AttackComponent.h"
#include "AugmentDamageLibrary.h"
#include "DefenceComponent.h"
#include "HealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
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
    bBerserkerActivated = false;
}

//이번 공격으로 줄 데미지를 계산 평타 데미지를 만들 때 씀
float UPassiveSkillsComponent::CalculateOutgoingDamage()
{
    if (!AttackComponent)
    {
        return 0.0f;
    }

    return AttackComponent->GetAttackPower();
}

//언리얼 데미지 시스템이 올려주는 데미지를 받아 방어력 체력 가시갑옷 흡혈을 처리
//UGameplayStatics::ApplyDamage -> AActor::TakeDamage -> OnTakeAnyDamage 순서로 여기까지 옴
void UPassiveSkillsComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
    if (!DefenceComponent || !HealthComponent)
    {
        return;
    }

    if (HealthComponent->bIsDead)
    {
        return;
    }

    if (Damage <= 0.0f)
    {
        return;
    }

    //방어력으로 깎되 최소 보장치는 남김
    const float FinalDamage = FMath::Max(Damage - DefenceComponent->GetDefencePower(), MIN_DAMAGE);

    HealthComponent->ApplyDamage(FinalDamage);

    //반사로 들어온 데미지는 다시 반사하지 않고 상대를 회복시키지도 않음
    //서로 가시 갑옷을 들고 있을 때 무한히 주고받는 것을 막음
    if (UAugmentDamageLibrary::IsThornReflectDamage(DamageType))
    {
        return;
    }

    if (!DamageCauser)
    {
        return;
    }

    //자기가 자기를 때린 경우는 흡혈도 반사도 없음
    if (DamageCauser == DamagedActor)
    {
        return;
    }

    //때린 쪽은 실제로 얼마가 깎였는지 모르기 때문에 여기서 흡혈을 대신 걸어줌
    UPassiveSkillsComponent* CauserPassive = DamageCauser->FindComponentByClass<UPassiveSkillsComponent>();

    if (CauserPassive)
    {
        CauserPassive->ProcessOnDamageDealt(FinalDamage);
    }

    if (!bThornArmor)
    {
        return;
    }

    //가시 갑옷 반사 반사 표식을 달아서 되돌려 보냄
    UAugmentDamageLibrary::ApplyThornReflectDamage(
        GetOwner(),
        DamageCauser,
        FinalDamage * THORN_ARMOR_REFLECT_RATIO
    );
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

    if (HealthComponent)
    {
        HealthComponent->OnCurrentHealthChanged.AddDynamic(this, &UPassiveSkillsComponent::HandleCurrentHealthChanged);
    }

    AActor* OwnerActor = GetOwner();

    if (!OwnerActor)
    {
        return;
    }

    //이게 꺼져 있으면 데미지가 통째로 무시됨
    OwnerActor->SetCanBeDamaged(true);

    //여기 물려두면 5계층이 TakeDamage를 건드리지 않아도 방어력과 가시 갑옷이 자동으로 걸림
    OwnerActor->OnTakeAnyDamage.AddDynamic(this, &UPassiveSkillsComponent::HandleTakeAnyDamage);
}
