// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerOverallAugmentComponent.h"

#include "DispatchTableForPlayerAndBossComponent.h"

//생성자
UPlayerOverallAugmentComponent::UPlayerOverallAugmentComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    //Has-A 3계층 디스패치 테이블
    DispatchTableForPlayerAndBoss = CreateDefaultSubobject<UDispatchTableForPlayerAndBossComponent>(TEXT("DispatchTableForPlayerAndBoss"));

    BuildDefaultAugmentPool();
}

//기본 증강 풀을 채움 에디터에서 따로 채워두면 건너뜀
void UPlayerOverallAugmentComponent::BuildDefaultAugmentPool()
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
    AugmentPool.Add(FAugmentData(EAugmentID::LastFortress, EAugmentCategory::Passive, 8.0f, false));
    AugmentPool.Add(FAugmentData(EAugmentID::ThornArmor, EAugmentCategory::Passive, 8.0f, false));
    AugmentPool.Add(FAugmentData(EAugmentID::Vampire, EAugmentCategory::Passive, 8.0f, false));
    AugmentPool.Add(FAugmentData(EAugmentID::Regeneration, EAugmentCategory::Passive, 8.0f, false));

    AugmentPool.Add(FAugmentData(EAugmentID::SlowEnemy, EAugmentCategory::Active, 6.0f, false));
    AugmentPool.Add(FAugmentData(EAugmentID::AreaAttack, EAugmentCategory::Active, 6.0f, false));
    AugmentPool.Add(FAugmentData(EAugmentID::ContinuousAttack, EAugmentCategory::Active, 6.0f, false));

    //무기 증강은 무기 작업이 들어오면 EAugmentCategory::Weapon으로 여기에 추가
}

//번호로 증강 정보를 찾음 없으면 nullptr
FAugmentData* UPlayerOverallAugmentComponent::FindAugmentData(EAugmentID AugmentID)
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
bool UPlayerOverallAugmentComponent::PlayerDrawWeightedAugmentID(EAugmentID& OutAugmentID)
{
    float TotalWeight = 0.0f;

    for (const FAugmentData& AugmentData : AugmentPool)
    {
        if (AugmentData.Weight <= 0.0f)
        {
            continue;
        }

        TotalWeight += AugmentData.Weight;
    }

    if (TotalWeight <= 0.0f)
    {
        return false;
    }

    const float DrawPoint = FMath::FRandRange(0.0f, TotalWeight);

    float AccumulatedWeight = 0.0f;

    for (const FAugmentData& AugmentData : AugmentPool)
    {
        if (AugmentData.Weight <= 0.0f)
        {
            continue;
        }

        AccumulatedWeight += AugmentData.Weight;

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

//보상 UI 선택지용으로 겹치지 않게 여러 개를 뽑음 풀은 건드리지 않음
bool UPlayerOverallAugmentComponent::PlayerDrawAugmentChoices(TArray<EAugmentID>& OutAugmentIDs)
{
    OutAugmentIDs.Empty();

    //이번 선택지에서 아직 안 뽑힌 후보만 따로 모아둠
    TArray<const FAugmentData*> Candidates;

    for (const FAugmentData& AugmentData : AugmentPool)
    {
        if (AugmentData.Weight <= 0.0f)
        {
            continue;
        }

        Candidates.Add(&AugmentData);
    }

    while (OutAugmentIDs.Num() < AUGMENT_CHOICE_COUNT && Candidates.Num() > 0)
    {
        float TotalWeight = 0.0f;

        for (const FAugmentData* Candidate : Candidates)
        {
            TotalWeight += Candidate->Weight;
        }

        const float DrawPoint = FMath::FRandRange(0.0f, TotalWeight);

        //부동소수점 오차로 끝까지 못 찾았을 때는 마지막 후보
        int32 DrawnIndex = Candidates.Num() - 1;
        float AccumulatedWeight = 0.0f;

        for (int32 Index = 0; Index < Candidates.Num(); ++Index)
        {
            AccumulatedWeight += Candidates[Index]->Weight;

            if (DrawPoint <= AccumulatedWeight)
            {
                DrawnIndex = Index;
                break;
            }
        }

        const EAugmentID DrawnAugmentID = Candidates[DrawnIndex]->AugmentID;

        OutAugmentIDs.Add(DrawnAugmentID);

        //같은 증강이 선택지에 다시 나오지 않도록 후보에서 뺌
        //풀에 같은 번호가 여러 줄 있어도 전부 뺌
        Candidates.RemoveAll([DrawnAugmentID](const FAugmentData* Candidate)
            {
                return Candidate->AugmentID == DrawnAugmentID;
            });
    }

    return OutAugmentIDs.Num() > 0;
}

//증강을 적용하고 반복 획득 가능 여부가 false면 한 번 뽑힌 뒤 풀에서 제거
void UPlayerOverallAugmentComponent::ApplyAugment(EAugmentID AugmentID)
{
    ExecuteAugment(AugmentID);

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

//Category를 보고 디스패치 테이블로 실행을 위임
void UPlayerOverallAugmentComponent::ExecuteAugment(EAugmentID AugmentID)
{
    if (!DispatchTableForPlayerAndBoss)
    {
        return;
    }

    FAugmentData* FoundData = FindAugmentData(AugmentID);

    //풀에 없는 증강도 실행할 수 있게 번호로 분류를 유추
    const EAugmentCategory Category = FoundData ? FoundData->Category : GetAugmentCategory(AugmentID);

    //무기 증강은 아직 담당 컴포넌트가 없어서 여기서 멈춤
    //WeaponComponent가 생기면 이 자리에서 무기 쪽으로 위임
    if (Category == EAugmentCategory::Weapon)
    {
        return;
    }

    DispatchTableForPlayerAndBoss->ExecuteAugment(AugmentID);
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
