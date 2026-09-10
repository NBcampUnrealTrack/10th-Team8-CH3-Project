#include "HealthComponent.h"

//생성자

UHealthComponent::UHealthComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    bIsDead = false;
    MaxHealth = 100.0f;
    CurrentHealth = 0.0f;
}

//일반 함수

void UHealthComponent::ApplyDamage(float DamageAmount)
{
    if (bIsDead)
    {
        return;
    }

    SetCurrentHealth(CurrentHealth - DamageAmount);
}

void UHealthComponent::HealHealth(float HealAmount)
{
    if (bIsDead)
    {
        return;
    }

    SetCurrentHealth(CurrentHealth + HealAmount);
}

void UHealthComponent::SetCurrentHealth(float NewCurrentHealth)
{
    const float OldCurrentHealth = CurrentHealth;

    CurrentHealth = FMath::Clamp(NewCurrentHealth, 0.0f, MaxHealth);

    if (OldCurrentHealth == CurrentHealth)
    {
        return;
    }

    OnCurrentHealthChanged.Broadcast(OldCurrentHealth, CurrentHealth);

    if (CurrentHealth > 0.0f)
    {
        return;
    }

    bIsDead = true;

    OnDead.Broadcast();//시체 남기기 아이템 플러스 UI 등등 활용 
}

void UHealthComponent::SetMaxHealth(float NewMaxHealth)
{
    const float OldMaxHealth = MaxHealth;

    MaxHealth = FMath::Max(NewMaxHealth, 1.0f);

    if (OldMaxHealth == MaxHealth)
    {
        return;
    }

    OnMaxHealthChanged.Broadcast(OldMaxHealth, MaxHealth);
}

//UFUNCTION 함수

float UHealthComponent::GetCurrentHealth()
{
    return CurrentHealth;
}

float UHealthComponent::GetMaxHealth()
{
    return MaxHealth;
}

float UHealthComponent::GetHealthPercentage()
{
    if (MaxHealth <= 0.0f)
    {
        return 0.0f;
    }

    return CurrentHealth / MaxHealth;
}

//생명주기 함수

void UHealthComponent::BeginPlay()
{
    Super::BeginPlay();

    CurrentHealth = MaxHealth;
}