#include "AttackComponent.h"

//생성자
UAttackComponent::UAttackComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    BaseAttackPower = 0.0f;
    AttackPower = 0.0f;
}

//일반 함수

void UAttackComponent::AddAttackPower(float Amount)
{
    AttackPower = FMath::Max(AttackPower + Amount, 1.0f);
}

void UAttackComponent::MultiplyAttackPower(float Multiplier)
{
    AttackPower = FMath::Max(AttackPower * Multiplier, 1.0f);
}

//UFUNCTION 함수

float UAttackComponent::GetAttackPower()
{
    return AttackPower;
}

//가상 함수(이벤트)

void UAttackComponent::BeginPlay()
{
    Super::BeginPlay();

    AttackPower = BaseAttackPower;
}
