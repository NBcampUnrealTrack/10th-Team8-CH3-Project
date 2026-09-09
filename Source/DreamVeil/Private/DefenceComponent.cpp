#include "DefenceComponent.h"

//생성자

UDefenceComponent::UDefenceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    BaseDefencePower = 0.0f;
    DefencePower = 0.0f;
}

//일반 함수

void UDefenceComponent::AddDefencePower(float Amount)
{
    DefencePower = FMath::Max(DefencePower + Amount, 1.0f);
}

void UDefenceComponent::MultiplyDefencePower(float Multiplier)
{
    DefencePower = FMath::Max(DefencePower * Multiplier, 1.0f);
}

//UFUNCTION 함수

float UDefenceComponent::GetDefencePower()
{
    return DefencePower;
}

//가상 함수(이벤트)

void UDefenceComponent::BeginPlay()
{
    Super::BeginPlay();

    DefencePower = BaseDefencePower;
}