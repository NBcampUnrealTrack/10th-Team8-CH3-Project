#include "CombatStatsComponent.h"

#include "AugmentTypes.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"

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

// 증강 이펙트
// 증강 스킬은 UObject라 월드에 이펙트를 붙일 수 없어서 이 컴포넌트가 대신 재생해줌

//가시 갑옷 반사를 맞았다는 연출
void UCombatStatsComponent::PlayThornReflectEffect()
{
    AActor* OwnerActor = GetOwner();

    if (!ThornReflectEffect || !OwnerActor)
    {
        return;
    }

    //액터의 루트에 붙여서 재생 발밑에서 가시가 솟는 그림이라 원점(발 기준)에 둠
    //붙여서 재생하는 이유 맞고 뒤로 밀려나도 가시가 몸을 따라가게 하려는 것
    //bAutoDestroy가 true라 재생이 끝나면 알아서 사라짐 Loop Behavior는 Once로 만들 것
    UNiagaraFunctionLibrary::SpawnSystemAttached(
        ThornReflectEffect,
        OwnerActor->GetRootComponent(),
        NAME_None,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        EAttachLocation::SnapToTarget,
        true
    );
}

//폭발 연출을 지정한 위치에 재생
void UCombatStatsComponent::PlayAreaAttackEffect(const FVector& Location)
{
    if (!AreaAttackEffect)
    {
        return;
    }

    //터진 자리에 남기는 연출이라 붙이지 않고 월드 좌표에 그대로 스폰함
    //쏜 사람이 움직여도 폭발은 그 자리에 있어야 맞음
    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        GetWorld(),
        AreaAttackEffect,
        Location
    );
}

//불타는 상태를 켜고 끔
void UCombatStatsComponent::SetOnFire(bool bNewOnFire)
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

        if (!OnFireEffect || !OwnerActor)
        {
            return;
        }

        //끌 때 없애야 하므로 스폰한 컴포넌트를 들고 있음
        //bAutoDestroy를 false로 두는 이유 무한 반복이라 스스로 끝나지 않고 우리가 꺼야 함
        OnFireEffectComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
            OnFireEffect,
            OwnerActor->GetRootComponent(),
            NAME_None,
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            EAttachLocation::SnapToTarget,
            false
        );

        return;
    }

    //불이 꺼짐 남은 불꽃을 정리함
    if (OnFireEffectComponent)
    {
        //Deactivate가 아니라 DestroyComponent를 쓰는 이유 다시 불이 붙으면 새로 스폰하므로 남겨둘 이유가 없음
        OnFireEffectComponent->DestroyComponent();
        OnFireEffectComponent = nullptr;
    }
}

//지금 불타고 있는지
bool UCombatStatsComponent::IsOnFire() const
{
    return bOnFire;
}
