// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerOverallAugmentComponent.h"

//생성자
UPlayerOverallAugmentComponent::UPlayerOverallAugmentComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

//플레이어 가중치로 증강 하나를 뽑음 보상 UI에 후보를 띄울 때 씀
bool UPlayerOverallAugmentComponent::PlayerDrawWeightedAugmentID(EAugmentID& OutAugmentID)
{
    return DrawWeightedAugmentID(OutAugmentID);
}

//겹치지 않는 후보 여러 개를 뽑음 보상 UI에 선택지 세 개를 띄울 때 씀
void UPlayerOverallAugmentComponent::PlayerDrawAugmentChoices(int32 ChoiceCount, TArray<EAugmentID>& OutAugmentIDs)
{
    OutAugmentIDs.Empty();

    if (ChoiceCount <= 0)
    {
        return;
    }

    //같은 후보가 두 번 뜨지 않게 뽑을 때마다 다시 시도
    //풀이 후보 수보다 적으면 있는 만큼만 반환
    const int32 MaxTryCount = ChoiceCount * 20;

    int32 TryCount = 0;

    while (OutAugmentIDs.Num() < ChoiceCount && TryCount < MaxTryCount)
    {
        ++TryCount;

        EAugmentID DrawnAugmentID = EAugmentID::AttackUp;

        if (!PlayerDrawWeightedAugmentID(DrawnAugmentID))
        {
            return;
        }

        if (OutAugmentIDs.Contains(DrawnAugmentID))
        {
            continue;
        }

        OutAugmentIDs.Add(DrawnAugmentID);
    }
}

//뽑아서 바로 적용 외부 진입점
bool UPlayerOverallAugmentComponent::DrawAndApplyAugment()
{
    EAugmentID DrawnAugmentID = EAugmentID::AttackUp;

    if (!PlayerDrawWeightedAugmentID(DrawnAugmentID))
    {
        return false;
    }

    ApplyAugment(DrawnAugmentID);

    return true;
}
