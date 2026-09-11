// Fill out your copyright notice in the Description page of Project Settings.


#include "OverallSkillsComponent.h"

#include "ActiveSkillsComponent.h"
#include "PassiveSkillsComponent.h"

//생성자
UOverallSkillsComponent::UOverallSkillsComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    PassiveSkillsComponent = nullptr;
    ActiveSkillsComponent = nullptr;

    BuildDefaultAugmentPool();
}

//기본 증강 풀을 채움 에디터에서 따로 채워두면 건너뜀
void UOverallSkillsComponent::BuildDefaultAugmentPool()
{
    if (AugmentPool.Num() > 0)
    {
        return;
    }

    //스탯 증가 계열은 여러 번 먹을 수 있게 중첩 허용
    AugmentPool.Add(FAugmentData(EAugmentID::AttackUp, EAugmentCategory::Passive, 20.0f, true));
    AugmentPool.Add(FAugmentData(EAugmentID::DefenceUp, EAugmentCategory::Passive, 20.0f, true));
    AugmentPool.Add(FAugmentData(EAugmentID::HealthUp, EAugmentCategory::Passive, 20.0f, true));

    //특수 효과 계열은 한 번만
    AugmentPool.Add(FAugmentData(EAugmentID::Berserker, EAugmentCategory::Passive, 8.0f, false));
    AugmentPool.Add(FAugmentData(EAugmentID::ThornArmor, EAugmentCategory::Passive, 8.0f, false));
    AugmentPool.Add(FAugmentData(EAugmentID::Vampire, EAugmentCategory::Passive, 8.0f, false));
    AugmentPool.Add(FAugmentData(EAugmentID::Regeneration, EAugmentCategory::Passive, 8.0f, false));

    //액티브도 한 번만
    AugmentPool.Add(FAugmentData(EAugmentID::SlowEnemy, EAugmentCategory::Active, 6.0f, false));
    AugmentPool.Add(FAugmentData(EAugmentID::AreaAttack, EAugmentCategory::Active, 6.0f, false));
    AugmentPool.Add(FAugmentData(EAugmentID::ContinuousAttack, EAugmentCategory::Active, 6.0f, false));

    //무기 증강은 무기 작업이 들어오면 여기에 EAugmentCategory::Weapon으로 추가
}

//소유 액터에 붙어 있는 패시브 액티브 컴포넌트를 찾음
//두 컴포넌트는 소유 액터가 직접 들고 있어야 함 여기서 만들지 않는 이유는
//패시브가 공방체를 서브오브젝트로 들고 있어서 런타임 생성이면 등록이 꼬이기 때문
void UOverallSkillsComponent::FindSkillComponents()
{
    AActor* OwnerActor = GetOwner();

    if (!OwnerActor)
    {
        return;
    }

    PassiveSkillsComponent = OwnerActor->FindComponentByClass<UPassiveSkillsComponent>();

    if (!PassiveSkillsComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("OverallSkillsComponent : PassiveSkillsComponent not found on %s"), *OwnerActor->GetName());
    }

    ActiveSkillsComponent = OwnerActor->FindComponentByClass<UActiveSkillsComponent>();

    if (!ActiveSkillsComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("OverallSkillsComponent : ActiveSkillsComponent not found on %s"), *OwnerActor->GetName());
    }
}

//번호로 증강 정보를 찾음 없으면 nullptr
FAugmentData* UOverallSkillsComponent::FindAugmentData(EAugmentID AugmentID)
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
bool UOverallSkillsComponent::DrawWeightedAugmentID(EAugmentID& OutAugmentID)
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

//증강을 적용하고 비중첩이면 풀에서 제거
void UOverallSkillsComponent::ApplyAugment(EAugmentID AugmentID)
{
    FAugmentData* FoundData = FindAugmentData(AugmentID);

    ExecuteAugment(AugmentID);

    OnAugmentApplied.Broadcast(AugmentID);

    if (!FoundData)
    {
        return;
    }

    if (FoundData->bStackable)
    {
        return;
    }

    AugmentPool.RemoveAll([AugmentID](const FAugmentData& AugmentData)
        {
            return AugmentData.AugmentID == AugmentID;
        });
}

//분류를 보고 패시브 액티브 무기 중 맞는 쪽으로 위임
void UOverallSkillsComponent::ExecuteAugment(EAugmentID AugmentID)
{
    FAugmentData* FoundData = FindAugmentData(AugmentID);

    //풀에 없는 증강도 실행할 수 있게 번호로 분류를 유추
    const EAugmentCategory Category = FoundData ? FoundData->Category : GetAugmentCategory(AugmentID);

    if (Category == EAugmentCategory::Passive)
    {
        if (!PassiveSkillsComponent)
        {
            return;
        }

        PassiveSkillsComponent->ExecutePassiveAugment(AugmentID);
        return;
    }

    if (Category == EAugmentCategory::Active)
    {
        if (!ActiveSkillsComponent)
        {
            return;
        }

        ActiveSkillsComponent->ExecuteActiveAugment(AugmentID);
        return;
    }

    //무기 증강 확장 지점
    //무기 컴포넌트가 생기면 여기서 바로 부르고 그 전까지는 이벤트로만 알림
    if (Category == EAugmentCategory::Weapon)
    {
        OnWeaponAugmentExecuted.Broadcast(AugmentID);
    }
}

//뽑아서 바로 적용 보상 단계에서 부르는 외부 진입점
bool UOverallSkillsComponent::DrawAndApplyAugment()
{
    EAugmentID DrawnAugmentID = EAugmentID::AttackUp;

    if (!DrawWeightedAugmentID(DrawnAugmentID))
    {
        return false;
    }

    ApplyAugment(DrawnAugmentID);

    return true;
}

//정해진 증강 묶음을 한 번에 적용 일반 정예 몬스터 고정 세트용
void UOverallSkillsComponent::ApplyFixedAugments(const TArray<EAugmentID>& AugmentIDs)
{
    for (EAugmentID AugmentID : AugmentIDs)
    {
        ApplyAugment(AugmentID);
    }
}

//풀에 남은 증강 개수
int32 UOverallSkillsComponent::GetRemainingAugmentCount()
{
    return AugmentPool.Num();
}

//액티브가 올린 데미지 요청을 그대로 위로 넘김
void UOverallSkillsComponent::HandleActiveDamageRequested(const TArray<AActor*>& Targets, float Damage)
{
    //여기서 바로 때리지 않고 위로 넘김
    //플레이어 및 데미지 쪽이 정리되면 그쪽에서 이 이벤트를 받아 실제 데미지를 넣음
    OnAugmentDamageRequested.Broadcast(Targets, Damage);
}

//생명주기 함수
void UOverallSkillsComponent::BeginPlay()
{
    Super::BeginPlay();

    FindSkillComponents();

    if (!ActiveSkillsComponent)
    {
        return;
    }

    ActiveSkillsComponent->OnActiveDamageRequested.AddDynamic(this, &UOverallSkillsComponent::HandleActiveDamageRequested);
}
