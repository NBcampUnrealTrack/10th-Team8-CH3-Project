#include "AttackComponent.h"

//생성자

UAttackComponent::UAttackComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    BaseAttackPower = 0.0f;
    AdditionalAttackPower = 0.0f;
    AttackMultiplier = 1.0f;
    AttackPower = 0.0f;
}

//일반 함수

void UAttackComponent::AddAttackPower(float Amount)
{
    AdditionalAttackPower += Amount;

    SetAttackPower();
}

void UAttackComponent::MultiplyAttackPower(float Multiplier)
{
    AttackMultiplier *= Multiplier;

    SetAttackPower();
}

void UAttackComponent::SetAttackPower()
{
    AttackPower = FMath::Max(
        (BaseAttackPower + AdditionalAttackPower) * AttackMultiplier,
        1.0f
    );
}

//UFUNCTION 함수

float UAttackComponent::GetAttackPower()
{
    return AttackPower;
}

//생명주기 함수

void UAttackComponent::BeginPlay()
{
    Super::BeginPlay();

    SetAttackPower();
}