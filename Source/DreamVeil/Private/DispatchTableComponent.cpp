#include "DispatchTableComponent.h"

#include "ActiveAugmentSkills.h"
#include "AugmentSkillBase.h"
#include "CombatStatsComponent.h"
#include "GameFramework/Actor.h"
#include "PassiveAugmentSkills.h"

//생성자
UDispatchTableComponent::UDispatchTableComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    RegisterSkillClasses();

    //블루프린트에서 목록을 따로 채워두면 저장된 값이 이걸 덮어씀
    AugmentPool.BuildDefault();
}

//증강 번호와 스킬 클래스를 테이블에 등록
void UDispatchTableComponent::RegisterSkillClasses()
{
    //패시브
    SkillClassTable.Add(EAugmentID::AttackUp, UAttackUpSkill::StaticClass());
    SkillClassTable.Add(EAugmentID::DefenceUp, UDefenceUpSkill::StaticClass());
    SkillClassTable.Add(EAugmentID::HealthUp, UHealthUpSkill::StaticClass());
    SkillClassTable.Add(EAugmentID::Berserker, UBerserkerSkill::StaticClass());
    SkillClassTable.Add(EAugmentID::LastFortress, ULastFortressSkill::StaticClass());
    SkillClassTable.Add(EAugmentID::ThornArmor, UThornArmorSkill::StaticClass());
    SkillClassTable.Add(EAugmentID::Vampire, UVampireSkill::StaticClass());
    SkillClassTable.Add(EAugmentID::Regeneration, URegenerationSkill::StaticClass());

    //액티브
    SkillClassTable.Add(EAugmentID::SlowEnemy, USlowEnemySkill::StaticClass());
    SkillClassTable.Add(EAugmentID::AreaAttack, UAreaAttackSkill::StaticClass());
    SkillClassTable.Add(EAugmentID::ContinuousAttack, UContinuousAttackSkill::StaticClass());

    //무기 증강 스킬이 생기면 여기에 한 줄씩 추가
}

//같은 액터의 스탯 컴포넌트
UCombatStatsComponent* UDispatchTableComponent::GetStatsComponent() const
{
    return StatsComponent;
}

//보상 UI 선택지를 겹치지 않게 뽑음
bool UDispatchTableComponent::DrawAugmentChoices(TArray<EAugmentID>& OutAugmentIDs)
{
    return AugmentPool.DrawChoices(AUGMENT_CHOICE_COUNT, OutAugmentIDs);
}

//증강을 적용하고 반복 획득이 안 되면 풀에서 제거
bool UDispatchTableComponent::ApplyAugment(EAugmentID AugmentID)
{
    if (!ExecuteAugment(AugmentID))
    {
        return false;
    }

    AugmentPool.RemoveIfNotRepeatable(AugmentID);

    AugmentHistory.Add(AugmentID);

    return true;
}

//풀에서 하나 뽑아서 바로 적용
bool UDispatchTableComponent::DrawAndApplyAugment()
{
    TArray<EAugmentID> DrawnAugmentIDs;

    if (!AugmentPool.DrawChoices(1, DrawnAugmentIDs))
    {
        return false;
    }

    return ApplyAugment(DrawnAugmentIDs[0]);
}

//풀과 상관없이 증강 효과만 실행
bool UDispatchTableComponent::ExecuteAugment(EAugmentID AugmentID)
{
    //스킬 객체는 월드에서 만들어야 하고 스탯 컴포넌트도 BeginPlay에서 찾으므로 그 전에는 막음
    if (!HasBegunPlay())
    {
        UE_LOG(LogTemp, Warning, TEXT("DispatchTableComponent: apply augments after BeginPlay (%s)"), *GetNameSafe(GetOwner()));
        return false;
    }

    UAugmentSkillBase* Skill = FindOrCreateSkill(AugmentID);

    //무기 증강처럼 테이블에 스킬이 없는 번호
    if (!Skill)
    {
        return false;
    }

    Skill->Apply();

    return true;
}

//정해진 증강 묶음을 한 번에 실행
void UDispatchTableComponent::ExecuteAugments(const TArray<EAugmentID>& AugmentIDs)
{
    for (EAugmentID AugmentID : AugmentIDs)
    {
        ExecuteAugment(AugmentID);
    }
}

//같은 증강을 여러 번 실행
void UDispatchTableComponent::ExecuteAugmentRepeat(EAugmentID AugmentID, int32 RepeatCount)
{
    for (int32 Index = 0; Index < RepeatCount; ++Index)
    {
        ExecuteAugment(AugmentID);
    }
}

//이미 얻은 증강인지 여부
bool UDispatchTableComponent::HasAcquiredAugment(EAugmentID AugmentID) const
{
    return AcquiredSkills.Contains(AugmentID);
}

//ApplyAugment로 얻은 증강 기록
TArray<EAugmentID> UDispatchTableComponent::GetAugmentHistory() const
{
    return AugmentHistory;
}

//저장해둔 기록대로 증강을 다시 얻음
bool UDispatchTableComponent::RestoreAugments(const TArray<EAugmentID>& History)
{
    //스킬 객체와 스탯 컴포넌트가 준비된 뒤여야 함
    if (!HasBegunPlay())
    {
        UE_LOG(LogTemp, Warning, TEXT("DispatchTableComponent: restore augments after BeginPlay (%s)"), *GetNameSafe(GetOwner()));
        return false;
    }

    //두 번 부르면 공격력 증가 같은 반복 증강이 두 배로 쌓이므로 막음
    if (AugmentHistory.Num() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("DispatchTableComponent: augments already acquired, restore skipped (%s)"), *GetNameSafe(GetOwner()));
        return false;
    }

    //ApplyAugment를 거쳐야 반복 획득이 안 되는 증강이 풀에서도 빠지고 기록도 다시 쌓임
    for (EAugmentID AugmentID : History)
    {
        ApplyAugment(AugmentID);
    }

    return true;
}

//받은 데미지로 공격자에게 돌려줄 반사 데미지
float UDispatchTableComponent::CalculateThornReflectDamage(float FinalDamage)
{
    float ReflectDamage = 0.0f;

    for (UAugmentSkillBase* Skill : GetAcquiredSkillsSnapshot())
    {
        if (Skill)
        {
            ReflectDamage += Skill->CalculateReflectDamage(FinalDamage);
        }
    }

    return ReflectDamage;
}

//데미지를 입힌 뒤 보유 스킬 효과를 처리
void UDispatchTableComponent::ProcessOnDamageDealt(float FinalDamage)
{
    for (UAugmentSkillBase* Skill : GetAcquiredSkillsSnapshot())
    {
        //앞 스킬 처리 중에 주인이 죽어 정리됐으면 남은 스킬은 부르지 않음
        if (!HasBegunPlay())
        {
            break;
        }

        if (Skill)
        {
            Skill->OnDamageDealt(FinalDamage);
        }
    }
}

//무기가 무언가를 맞혔을 때 보유 스킬 효과를 처리
void UDispatchTableComponent::ProcessWeaponHit(const FHitResult& HitResult, float HitDamage)
{
    for (UAugmentSkillBase* Skill : GetAcquiredSkillsSnapshot())
    {
        //범위 공격의 가시 갑옷 반사로 주인이 죽어 정리됐으면 남은 스킬은 부르지 않음
        if (!HasBegunPlay())
        {
            break;
        }

        if (Skill)
        {
            Skill->OnWeaponHit(HitResult, HitDamage);
        }
    }
}

//스탯 컴포넌트의 체력 변화를 받아 스킬에게 전달
void UDispatchTableComponent::HandleCurrentHealthChanged(float OldValue, float NewValue)
{
    for (UAugmentSkillBase* Skill : GetAcquiredSkillsSnapshot())
    {
        //앞 스킬 처리 중에 주인이 정리됐으면 남은 스킬은 부르지 않음
        if (!HasBegunPlay())
        {
            break;
        }

        if (Skill)
        {
            Skill->OnHealthChanged(OldValue, NewValue);
        }
    }
}

//얻은 스킬 목록을 배열로 복사 스킬 객체 자체가 아니라 포인터만 복사함
TArray<TObjectPtr<UAugmentSkillBase>> UDispatchTableComponent::GetAcquiredSkillsSnapshot() const
{
    TArray<TObjectPtr<UAugmentSkillBase>> Skills;

    AcquiredSkills.GenerateValueArray(Skills);

    return Skills;
}

//얻은 스킬이 있으면 돌려주고 없으면 테이블을 보고 새로 만듦
UAugmentSkillBase* UDispatchTableComponent::FindOrCreateSkill(EAugmentID AugmentID)
{
    TObjectPtr<UAugmentSkillBase>* FoundSkill = AcquiredSkills.Find(AugmentID);

    if (FoundSkill && *FoundSkill)
    {
        return *FoundSkill;
    }

    TSubclassOf<UAugmentSkillBase>* SkillClass = SkillClassTable.Find(AugmentID);

    if (!SkillClass || !*SkillClass)
    {
        return nullptr;
    }

    //Outer를 이 컴포넌트로 넣어서 스킬이 주인과 월드를 찾을 수 있게 함
    UAugmentSkillBase* NewSkill = NewObject<UAugmentSkillBase>(this, *SkillClass);

    AcquiredSkills.Add(AugmentID, NewSkill);

    return NewSkill;
}

//생명주기 함수
void UDispatchTableComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* OwnerActor = GetOwner();

    if (OwnerActor)
    {
        StatsComponent = OwnerActor->FindComponentByClass<UCombatStatsComponent>();
    }

    //스탯 컴포넌트가 없으면 스탯을 바꾸는 증강은 아무 효과가 없음
    if (!StatsComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("DispatchTableComponent: CombatStatsComponent is missing on %s"), *GetNameSafe(OwnerActor));
        return;
    }

    StatsComponent->OnCurrentHealthChanged.AddDynamic(this, &UDispatchTableComponent::HandleCurrentHealthChanged);
}

void UDispatchTableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (StatsComponent)
    {
        StatsComponent->OnCurrentHealthChanged.RemoveDynamic(this, &UDispatchTableComponent::HandleCurrentHealthChanged);
    }

    //스킬이 걸어둔 타이머와 효과를 정리
    for (TPair<EAugmentID, TObjectPtr<UAugmentSkillBase>>& SkillPair : AcquiredSkills)
    {
        if (SkillPair.Value)
        {
            SkillPair.Value->Deactivate();
        }
    }

    AcquiredSkills.Empty();

    Super::EndPlay(EndPlayReason);
}
