// Fill out your copyright notice in the Description page of Project Settings.


#include "BossMonsterOverallAugmentComponent.h"

//생성자
UBossMonsterOverallAugmentComponent::UBossMonsterOverallAugmentComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    WaveLevel = 0;
    StatWeightPerWaveLevel = 5.0f;
}

//웨이브 레벨을 설정
void UBossMonsterOverallAugmentComponent::SetWaveLevel(int32 NewWaveLevel)
{
    WaveLevel = FMath::Max(NewWaveLevel, 0);
}

//뽑기에 쓸 실제 가중치 웨이브 레벨이 오를수록 공방체 증가 쪽을 밀어줌
float UBossMonsterOverallAugmentComponent::GetDrawWeight(const FAugmentData& AugmentData)
{
    //반복 획득이 가능한 증강만 웨이브 레벨 보정을 받음
    //후반 웨이브에서 한 번짜리 증강이 다 빠지고 나면 수치 증강으로 계속 강해지게 하려는 것
    if (!AugmentData.bRepeatable)
    {
        return AugmentData.Weight;
    }

    return AugmentData.Weight + StatWeightPerWaveLevel * WaveLevel;
}

//보스 가중치로 증강 하나를 뽑음
bool UBossMonsterOverallAugmentComponent::BossWeightedAugmentID(EAugmentID& OutAugmentID)
{
    return DrawWeightedAugmentID(OutAugmentID);
}

//뽑아서 바로 적용 외부 진입점
bool UBossMonsterOverallAugmentComponent::RandomApplyAugment()
{
    EAugmentID DrawnAugmentID = EAugmentID::AttackUp;

    if (!BossWeightedAugmentID(DrawnAugmentID))
    {
        return false;
    }

    ApplyAugment(DrawnAugmentID);

    return true;
}

//웨이브가 넘어갈 때 레벨을 올리고 그만큼 증강을 뽑아 적용
void UBossMonsterOverallAugmentComponent::AdvanceWave(int32 NewWaveLevel, int32 AugmentCount)
{
    SetWaveLevel(NewWaveLevel);

    for (int32 Index = 0; Index < AugmentCount; ++Index)
    {
        RandomApplyAugment();
    }
}
