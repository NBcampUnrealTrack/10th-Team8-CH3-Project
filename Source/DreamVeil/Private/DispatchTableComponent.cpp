#include "DispatchTableComponent.h"

#include "ActiveAugmentSkills.h"
#include "AugmentSkillBase.h"
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

//스탯 전체 복사본
FCombatStats UDispatchTableComponent::GetStats() const
{
    return Stats;
}

//최종 공격력
float UDispatchTableComponent::GetAttackPower() const
{
    return Stats.GetAttackPower();
}

//최종 방어력
float UDispatchTableComponent::GetDefencePower() const
{
    return Stats.GetDefencePower();
}

//현재 체력
float UDispatchTableComponent::GetCurrentHealth() const
{
    return Stats.CurrentHealth;
}

//최대 체력
float UDispatchTableComponent::GetMaxHealth() const
{
    return Stats.MaxHealth;
}

//체력 비율 0~1
float UDispatchTableComponent::GetHealthPercentage() const
{
    return Stats.GetHealthPercentage();
}

//죽었는지 여부
bool UDispatchTableComponent::IsDead() const
{
    return Stats.bIsDead;
}

//공방체 기본값을 넣고 죽음 상태를 풀고 체력을 가득 채움
void UDispatchTableComponent::InitStats(float NewMaxHealth, float NewDefencePower, float NewAttackPower)
{
    //최종값은 매번 계산하므로 기본값만 바꾸면 바로 반영됨
    Stats.BaseDefencePower = NewDefencePower;
    Stats.BaseAttackPower = NewAttackPower;

    SetMaxHealth(NewMaxHealth);

    ResetHealth();
}

//추가 공격력을 더함
void UDispatchTableComponent::AddAttackPower(float Amount)
{
    Stats.AdditionalAttackPower += Amount;
}

//공격력 배율을 곱함
void UDispatchTableComponent::MultiplyAttackPower(float Multiplier)
{
    Stats.AttackMultiplier *= Multiplier;
}

//추가 방어력을 더함
void UDispatchTableComponent::AddDefencePower(float Amount)
{
    Stats.AdditionalDefencePower += Amount;
}

//방어력 배율을 곱함
void UDispatchTableComponent::MultiplyDefencePower(float Multiplier)
{
    Stats.DefenceMultiplier *= Multiplier;
}

//최대 체력을 바꿈 현재 체력이 더 크면 최대 체력까지 줄임
void UDispatchTableComponent::SetMaxHealth(float NewMaxHealth)
{
    const float OldMaxHealth = Stats.MaxHealth;

    Stats.MaxHealth = FMath::Max(NewMaxHealth, 1.0f);

    if (OldMaxHealth != Stats.MaxHealth)
    {
        OnMaxHealthChanged.Broadcast(OldMaxHealth, Stats.MaxHealth);
    }

    if (Stats.CurrentHealth > Stats.MaxHealth)
    {
        SetCurrentHealth(Stats.MaxHealth);
    }
}

//현재 체력을 바꿈 0이면 사망 0보다 크면 살아 있는 상태로 되돌림
void UDispatchTableComponent::SetCurrentHealth(float NewCurrentHealth)
{
    const float OldCurrentHealth = Stats.CurrentHealth;
    const bool bWasDead = Stats.bIsDead;

    Stats.CurrentHealth = FMath::Clamp(NewCurrentHealth, 0.0f, Stats.MaxHealth);

    if (OldCurrentHealth == Stats.CurrentHealth)
    {
        return;
    }

    //방송 전에 죽음 상태부터 맞춰둠 받는 쪽이 체력 0인데 살아 있는 상태를 보지 않도록
    Stats.bIsDead = Stats.CurrentHealth <= 0.0f;

    //광전사 최후의 요새 같은 스킬이 체력 변화를 먼저 반영
    for (TPair<EAugmentID, TObjectPtr<UAugmentSkillBase>>& SkillPair : AcquiredSkills)
    {
        if (SkillPair.Value)
        {
            SkillPair.Value->OnHealthChanged(OldCurrentHealth, Stats.CurrentHealth);
        }
    }

    OnCurrentHealthChanged.Broadcast(OldCurrentHealth, Stats.CurrentHealth);

    if (Stats.bIsDead && !bWasDead)
    {
        OnDead.Broadcast();
    }
}

//체력 회복 죽은 상태면 무시
void UDispatchTableComponent::Heal(float HealAmount)
{
    if (Stats.bIsDead)
    {
        return;
    }

    SetCurrentHealth(Stats.CurrentHealth + HealAmount);
}

//죽음 상태를 풀고 체력을 가득 채움
void UDispatchTableComponent::ResetHealth()
{
    Stats.bIsDead = false;

    SetCurrentHealth(Stats.MaxHealth);
}

//이번 공격으로 줄 데미지
float UDispatchTableComponent::CalculateOutgoingDamage() const
{
    return Stats.GetAttackPower();
}

//받은 데미지를 방어력으로 줄여 체력에 적용하고 적용한 데미지를 반환
float UDispatchTableComponent::ApplyIncomingDamage(float IncomingDamage)
{
    if (Stats.bIsDead)
    {
        return 0.0f;
    }

    if (IncomingDamage <= 0.0f)
    {
        return 0.0f;
    }

    //방어력으로 깎되 최소 보장치는 남김 남은 체력보다 커도 자르지 않음(오버킬 허용)
    const float FinalDamage = FMath::Max(IncomingDamage - Stats.GetDefencePower(), MIN_DAMAGE);

    SetCurrentHealth(Stats.CurrentHealth - FinalDamage);

    return FinalDamage;
}

//받은 데미지로 공격자에게 돌려줄 반사 데미지
float UDispatchTableComponent::CalculateThornReflectDamage(float FinalDamage)
{
    float ReflectDamage = 0.0f;

    for (TPair<EAugmentID, TObjectPtr<UAugmentSkillBase>>& SkillPair : AcquiredSkills)
    {
        if (SkillPair.Value)
        {
            ReflectDamage += SkillPair.Value->CalculateReflectDamage(FinalDamage);
        }
    }

    return ReflectDamage;
}

//데미지를 입힌 뒤 보유 스킬 효과를 처리
void UDispatchTableComponent::ProcessOnDamageDealt(float FinalDamage)
{
    for (TPair<EAugmentID, TObjectPtr<UAugmentSkillBase>>& SkillPair : AcquiredSkills)
    {
        if (SkillPair.Value)
        {
            SkillPair.Value->OnDamageDealt(FinalDamage);
        }
    }
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
    //스킬 객체는 월드에서 만들어야 해서 BeginPlay 전에 불리면 막음
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

    //시작할 때 체력을 가득 채움 이벤트는 보내지 않으므로 UI는 Getter로 시작 값을 읽을 것
    Stats.CurrentHealth = Stats.MaxHealth;
    Stats.bIsDead = false;
}

void UDispatchTableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
