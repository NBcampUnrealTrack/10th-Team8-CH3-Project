// Fill out your copyright notice in the Description page of Project Settings.


#include "OverallAugmentComponent.h"

#include "DispatchTableForPlayerAndBossComponent.h"

//생성자
UOverallAugmentComponent::UOverallAugmentComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    //Has-A 3계층 디스패치 테이블
    DispatchTableForPlayerAndBoss = CreateDefaultSubobject<UDispatchTableForPlayerAndBossComponent>(TEXT("DispatchTableForPlayerAndBoss"));

    BuildDefaultAugmentPool();
}

//기본 증강 풀을 채움 에디터에서 따로 채워두면 건너뜀
void UOverallAugmentComponent::BuildDefaultAugmentPool()
{
    if (AugmentPool.Num() > 0)
    {
        return;
    }

    //공방체 증가는 먹을 때마다 수치가 더해지므로 반복 획득 허용
    AugmentPool.Add(FAugmentData(EAugmentID::AttackUp, EAugmentCategory::Passive, 20.0f, true));
    AugmentPool.Add(FAugmentData(EAugmentID::DefenceUp, EAugmentCategory::Passive, 20.0f, true));
    AugmentPool.Add(FAugmentData(EAugmentID::HealthUp, EAugmentCategory::Passive, 20.0f, true));

    //2계층에 중복 방지 bool이 있는 증강은 두 번째가 무시되므로 반복 획득 금지
    AugmentPool.Add(FAugmentData(EAugmentID::Berserker, EAugmentCategory::Passive, 8.0f, false));
    AugmentPool.Add(FAugmentData(EAugmentID::ThornArmor, EAugmentCategory::Passive, 8.0f, false));
    AugmentPool.Add(FAugmentData(EAugmentID::Vampire, EAugmentCategory::Passive, 8.0f, false));
    AugmentPool.Add(FAugmentData(EAugmentID::Regeneration, EAugmentCategory::Passive, 8.0f, false));

    AugmentPool.Add(FAugmentData(EAugmentID::SlowEnemy, EAugmentCategory::Active, 6.0f, false));
    AugmentPool.Add(FAugmentData(EAugmentID::AreaAttack, EAugmentCategory::Active, 6.0f, false));
    AugmentPool.Add(FAugmentData(EAugmentID::ContinuousAttack, EAugmentCategory::Active, 6.0f, false));

    //무기 증강은 무기 작업이 들어오면 EAugmentCategory::Weapon으로 여기에 추가
}

//뽑기에 쓸 실제 가중치 보스는 웨이브 레벨을 곱하려고 재정의함
float UOverallAugmentComponent::GetDrawWeight(const FAugmentData& AugmentData)
{
    return AugmentData.Weight;
}

//번호로 증강 정보를 찾음 없으면 nullptr
FAugmentData* UOverallAugmentComponent::FindAugmentData(EAugmentID AugmentID)
{
    for (FAugmentData& AugmentData : AugmentPool)
    {
        if (AugmentData.AugmentID == AugmentID)
        {
            return &AugmentData;
        }
    }

    return nullptr;
}

//가중치 누적합으로 증강 하나를 뽑음 성공하면 true
bool UOverallAugmentComponent::DrawWeightedAugmentID(EAugmentID& OutAugmentID)
{
    float TotalWeight = 0.0f;

    for (const FAugmentData& AugmentData : AugmentPool)
    {
        const float DrawWeight = GetDrawWeight(AugmentData);

        if (DrawWeight <= 0.0f)
        {
            continue;
        }

        TotalWeight += DrawWeight;
    }

    if (TotalWeight <= 0.0f)
    {
        return false;
    }

    const float DrawPoint = FMath::FRandRange(0.0f, TotalWeight);

    float AccumulatedWeight = 0.0f;

    for (const FAugmentData& AugmentData : AugmentPool)
    {
        const float DrawWeight = GetDrawWeight(AugmentData);

        if (DrawWeight <= 0.0f)
        {
            continue;
        }

        AccumulatedWeight += DrawWeight;

        if (DrawPoint <= AccumulatedWeight)
        {
            OutAugmentID = AugmentData.AugmentID;
            return true;
        }
    }

    //부동소수점 오차로 마지막을 넘겼을 때를 위한 처리
    OutAugmentID = AugmentPool.Last().AugmentID;

    return true;
}

//증강을 적용하고 반복 획득 불가면 풀에서 제거
void UOverallAugmentComponent::ApplyAugment(EAugmentID AugmentID)
{
    ExecuteAugment(AugmentID);

    OnAugmentApplied.Broadcast(AugmentID);

    FAugmentData* FoundData = FindAugmentData(AugmentID);

    if (!FoundData)
    {
        return;
    }

    //다시 뽑혀도 효과가 붙는 증강은 풀에 남겨둠
    if (FoundData->bRepeatable)
    {
        return;
    }

    //다시 뽑혀봐야 2계층에서 무시되는 증강은 보상 한 칸을 버리게 되므로 제거
    AugmentPool.RemoveAll([AugmentID](const FAugmentData& AugmentData)
        {
            return AugmentData.AugmentID == AugmentID;
        });
}

//3계층 디스패치 테이블로 실행을 위임
void UOverallAugmentComponent::ExecuteAugment(EAugmentID AugmentID)
{
    if (!DispatchTableForPlayerAndBoss)
    {
        return;
    }

    DispatchTableForPlayerAndBoss->ExecuteAugment(AugmentID);
}

//정해진 증강 묶음을 한 번에 적용 고정 세트용
void UOverallAugmentComponent::ApplyFixedAugments(const TArray<EAugmentID>& AugmentIDs)
{
    for (EAugmentID AugmentID : AugmentIDs)
    {
        ApplyAugment(AugmentID);
    }
}

//풀에 남은 증강 개수
int32 UOverallAugmentComponent::GetRemainingAugmentCount()
{
    return AugmentPool.Num();
}

//3계층 디스패치 테이블 Getter
UDispatchTableForPlayerAndBossComponent* UOverallAugmentComponent::GetDispatchTable()
{
    return DispatchTableForPlayerAndBoss;
}
