#include "AugmentPool.h"

#include "DispatchTableComponent.h"

//기본 증강 목록을 채움 이미 채워져 있으면 건너뜀
void FAugmentPool::BuildDefault()
{
    if (Augments.Num() > 0)
    {
        return;
    }

    //공방체 증가는 먹을 때마다 수치가 더해지므로 반복 획득 허용
    Augments.Add(FAugmentData(EAugmentID::AttackUp, EAugmentCategory::Passive, 20.0f, true));
    Augments.Add(FAugmentData(EAugmentID::DefenceUp, EAugmentCategory::Passive, 20.0f, true));
    Augments.Add(FAugmentData(EAugmentID::HealthUp, EAugmentCategory::Passive, 20.0f, true));

    //스태미나 증가도 먹을 때마다 더해지므로 반복 획득 허용 달리기 보조라서 공방체보다 덜 나오게 확률을 낮춤
    Augments.Add(FAugmentData(EAugmentID::StaminaUp, EAugmentCategory::Passive, 12.0f, true));

    //두 번째 획득부터 효과가 없는 증강은 반복 획득 금지
    Augments.Add(FAugmentData(EAugmentID::Berserker, EAugmentCategory::Passive, 8.0f, false));
    Augments.Add(FAugmentData(EAugmentID::LastFortress, EAugmentCategory::Passive, 8.0f, false));
    Augments.Add(FAugmentData(EAugmentID::ThornArmor, EAugmentCategory::Passive, 8.0f, false));
    Augments.Add(FAugmentData(EAugmentID::Vampire, EAugmentCategory::Passive, 8.0f, false));
    Augments.Add(FAugmentData(EAugmentID::Regeneration, EAugmentCategory::Passive, 8.0f, false));

    Augments.Add(FAugmentData(EAugmentID::Knockback, EAugmentCategory::Active, 6.0f, false));
    Augments.Add(FAugmentData(EAugmentID::AreaAttack, EAugmentCategory::Active, 6.0f, false));
    Augments.Add(FAugmentData(EAugmentID::ContinuousAttack, EAugmentCategory::Active, 6.0f, false));

    //무기 증강은 소총 권총 강화가 기획되면 EAugmentCategory::Weapon으로 여기에 추가

    FillDisplayTexts();
}

//보스가 뽑을 증강만 채움
void FAugmentPool::BuildBossDefault()
{
    //플레이어용 목록이 이미 들어 있을 수 있어서 비우고 다시 채움
    Augments.Empty();

    //공방체 증가는 먹을 때마다 수치가 더해지므로 반복 획득 허용
    //Endless에서 보스가 여러 번 나올수록 고유 증강이 바닥나고 여기에 계속 쌓임
    Augments.Add(FAugmentData(EAugmentID::AttackUp, EAugmentCategory::Passive, 20.0f, true));
    Augments.Add(FAugmentData(EAugmentID::DefenceUp, EAugmentCategory::Passive, 20.0f, true));
    Augments.Add(FAugmentData(EAugmentID::HealthUp, EAugmentCategory::Passive, 20.0f, true));

    //두 번째로 뽑혀도 효과가 없는 증강은 반복 획득 금지 한 번 뽑히면 풀에서 빠짐
    //보스에게 붙어도 말이 되는 것만 넣음 맞으면 반사하고 때리면 회복하고 몰리면 세지는 것들
    Augments.Add(FAugmentData(EAugmentID::Berserker, EAugmentCategory::Passive, 8.0f, false));
    Augments.Add(FAugmentData(EAugmentID::LastFortress, EAugmentCategory::Passive, 8.0f, false));
    Augments.Add(FAugmentData(EAugmentID::ThornArmor, EAugmentCategory::Passive, 8.0f, false));
    Augments.Add(FAugmentData(EAugmentID::Vampire, EAugmentCategory::Passive, 8.0f, false));
    Augments.Add(FAugmentData(EAugmentID::Regeneration, EAugmentCategory::Passive, 8.0f, false));

    //스태미나 넉백 범위공격 지속공격은 일부러 뺌 보스가 뽑으면 아무 일도 안 일어나는 빈 칸이 됨

    FillDisplayTexts();
}

//목록의 이름과 설명을 채움
void FAugmentPool::FillDisplayTexts()
{
    //이름과 설명은 한곳에서 관리하고 여기서 복사해옴 블루프린트에서 따로 고칠 수도 있음
    for (FAugmentData& AugmentData : Augments)
    {
        AugmentData.DisplayName = UDispatchTableComponent::GetAugmentDisplayName(AugmentData.AugmentID);
        AugmentData.Description = UDispatchTableComponent::GetAugmentDescription(AugmentData.AugmentID);
    }
}

//번호로 증강 정보를 찾음 없으면 nullptr
const FAugmentData* FAugmentPool::Find(EAugmentID AugmentID) const
{
    for (const FAugmentData& AugmentData : Augments)
    {
        if (AugmentData.AugmentID == AugmentID)
        {
            return &AugmentData;
        }
    }

    return nullptr;
}

//가중치 누적합으로 겹치지 않게 Count개를 뽑음 풀은 건드리지 않음
bool FAugmentPool::DrawChoices(int32 Count, TArray<EAugmentID>& OutAugmentIDs) const
{
    OutAugmentIDs.Empty();

    //이번 뽑기에서 아직 안 뽑힌 후보만 따로 모아둠
    TArray<const FAugmentData*> Candidates;

    for (const FAugmentData& AugmentData : Augments)
    {
        if (AugmentData.Weight <= 0.0f)
        {
            continue;
        }

        Candidates.Add(&AugmentData);
    }

    while (OutAugmentIDs.Num() < Count && Candidates.Num() > 0)
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

        //같은 증강이 다시 나오지 않도록 후보에서 뺌 같은 번호가 여러 줄 있어도 전부 뺌
        Candidates.RemoveAll([DrawnAugmentID](const FAugmentData* Candidate)
            {
                return Candidate->AugmentID == DrawnAugmentID;
            });
    }

    return OutAugmentIDs.Num() > 0;
}

//반복 획득이 안 되는 증강이면 풀에서 제거
void FAugmentPool::RemoveIfNotRepeatable(EAugmentID AugmentID)
{
    const FAugmentData* FoundData = Find(AugmentID);

    if (!FoundData)
    {
        return;
    }

    //다시 뽑혀도 효과가 붙는 증강은 풀에 남겨둠
    if (FoundData->bRepeatable)
    {
        return;
    }

    //다시 뽑혀봐야 효과가 없는 증강은 보상 한 칸을 버리게 되므로 제거
    Augments.RemoveAll([AugmentID](const FAugmentData& AugmentData)
        {
            return AugmentData.AugmentID == AugmentID;
        });
}
