#include "HealthComponent.h"

UHealthComponent::UHealthComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    MaxHealth = 0.0f;
    CurrentHealth=0.0f;
    bIsDead = false;
}

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
    MaxHealth = NewMaxHealth;
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

//가상함수

void UHealthComponent::BeginPlay()
{
    Super::BeginPlay();
    CurrentHealth = MaxHealth;
}