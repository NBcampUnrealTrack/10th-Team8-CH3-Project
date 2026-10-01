#include "CombatStatsComponent.h"

#include "AugmentTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

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

//공방체 기본값을 넣기 죽음 상태를 풀고 체력을 가득 채움
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

    //방어력이 높을수록 받는 피해가 비율로 줄어듦 방어력이 0이면 1이 되어 들어온 피해가 그대로 들어감
    //DEFENCE_REDUCTION_CONSTANT가 30이고 음수 방어력은 GetDefencePower가 막으므로 0으로 나눌 일은 없음
    const float DamageMultiplier = DEFENCE_REDUCTION_CONSTANT / (DEFENCE_REDUCTION_CONSTANT + DefencePower);

    //비율로 깎되 최소 보장치는 남김 남은 체력보다 커도 자르지 않음(오버킬 허용)
    const float FinalDamage = FMath::Max(IncomingDamage * DamageMultiplier, MIN_DAMAGE);

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

// 불타는 상태
// 연출 에셋은 불을 붙인 쪽의 DispatchTableComponent가 들고 있고 여기로 넘어옴

//불타는 상태를 켜고 끔
void UCombatStatsComponent::SetOnFire(bool bNewOnFire, UParticleSystem* FireEffect)
{
    //같은 상태로 또 부르면 아무것도 하지 않음 불꽃이 여러 개 겹치는 것을 막음
    if (bOnFire == bNewOnFire)
    {
        return;
    }

    bOnFire = bNewOnFire;

    if (bOnFire)
    {
        AActor* OwnerActor = GetOwner();

        //불꽃은 불을 붙인 쪽이 넘겨줌 안 넣어뒀으면 불꽃 없이 상태만 바뀜
        if (!FireEffect || !OwnerActor)
        {
            return;
        }

        //캡슐 원점은 몸 한가운데라 그대로 두면 불이 허리에서 시작함 발밑으로 내림
        FVector FootOffset = FVector::ZeroVector;

        if (const ACharacter* OwnerCharacter = Cast<ACharacter>(OwnerActor))
        {
            FootOffset.Z = -OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        }

        //몸에 붙여서 도망가도 불이 따라가게 함
        //bAutoDestroy를 false로 두는 이유 반복 재생이라 스스로 끝나지 않고 우리가 꺼야 함
        FireEffectComponent = UGameplayStatics::SpawnEmitterAttached(
            FireEffect,
            OwnerActor->GetRootComponent(),
            NAME_None,
            FootOffset,
            FRotator::ZeroRotator,
            EAttachLocation::KeepRelativeOffset,
            false
        );

        return;
    }

    //불이 꺼짐 남은 불꽃을 정리함
    if (FireEffectComponent)
    {
        //Deactivate가 아니라 DestroyComponent를 쓰는 이유 다시 붙으면 새로 스폰하므로 남겨둘 이유가 없음
        FireEffectComponent->DestroyComponent();
        FireEffectComponent = nullptr;
    }
}

//지금 불타고 있는지
bool UCombatStatsComponent::IsOnFire() const
{
    return bOnFire;
}
