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
    DefenceMultiplier *= Multiplier;//누적 구조로 갈것이냐 아니면 새로운 배율(누적 구조 생각중)

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