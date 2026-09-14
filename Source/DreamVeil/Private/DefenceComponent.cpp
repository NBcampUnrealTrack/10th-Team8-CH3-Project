#include "DefenceComponent.h"

//생성자

UDefenceComponent::UDefenceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    BaseDefencePower = 0.0f;
    AdditionalDefencePower = 0.0f;
    DefenceMultiplier = 1.0f;
    DefencePower = 0.0f;
}

//일반 함수

void UDefenceComponent::AddDefencePower(float Amount)
{
    AdditionalDefencePower += Amount;

    SetDefencePower();
}

void UDefenceComponent::MultiplyDefencePower(float Multiplier)
{
    //배율은 곱해서 누적 해제할 때는 역수를 곱함(최후의 요새)
    DefenceMultiplier *= Multiplier;

    SetDefencePower();
}


void UDefenceComponent::SetDefencePower()
{
    const float OldDefencePower = DefencePower;

    DefencePower = FMath::Max(
        (BaseDefencePower + AdditionalDefencePower) * DefenceMultiplier,
        1.0f
    );

    if (OldDefencePower != DefencePower)
    {
        OnDefencePowerChanged.Broadcast(OldDefencePower, DefencePower);
    }
}


//UFUNCTION 함수

float UDefenceComponent::GetDefencePower()
{
    return DefencePower;
}

//생명주기 함수

void UDefenceComponent::BeginPlay()
{
    Super::BeginPlay();

    SetDefencePower();
}