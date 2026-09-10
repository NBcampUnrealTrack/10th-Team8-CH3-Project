#include "HealthComponent.h"

//생성자

UHealthComponent::UHealthComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    MaxHealth = 0.0f;
    CurrentHealth=0.0f;
    bIsDead = false;
}

//일반함수

void UHealthComponent::HealHealth(float HealAmount)
{
    CurrentHealth += HealAmount;
    CurrentHealth = FMath::Min(CurrentHealth, MaxHealth);
}
void UHealthComponent::SetCurrentHealth(float NewHealth)
{
    CurrentHealth = NewHealth;
}

void UHealthComponent::SetMaxHealth(float NewMaxHealth)
{
    const float OldMaxHealth = MaxHealth;
    MaxHealth = NewMaxHealth;
    if (OldMaxHealth != MaxHealth)
    {
        OnMaxHealthChanged.Broadcast(OldMaxHealth, MaxHealth);
    }
}

//UFUNCTION 함수들

float UHealthComponent::GetMaxHealth()
{
    return MaxHealth;
}

float UHealthComponent::GetCurrentHealth()
{
    return CurrentHealth;
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