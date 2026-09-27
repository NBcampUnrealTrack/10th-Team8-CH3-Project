#include "DispatchTableComponent.h"

#include "ActiveAugmentSkills.h"
#include "AugmentSkillBase.h"
#include "CombatStatsComponent.h"
#include "PassiveAugmentSkills.h"
#include "ThornSpikeEffect.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"

//생성자
UDispatchTableComponent::UDispatchTableComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    //전용 가시 연출이 나오기 전까지 쓸 기본값 블루프린트에서 비우면 연출 없이 데미지만 들어감
    ThornSpikeEffectClass = AThornSpikeEffect::StaticClass();

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
    SkillClassTable.Add(EAugmentID::StaminaUp, UStaminaUpSkill::StaticClass());
    SkillClassTable.Add(EAugmentID::Berserker, UBerserkerSkill::StaticClass());
    SkillClassTable.Add(EAugmentID::LastFortress, ULastFortressSkill::StaticClass());
    SkillClassTable.Add(EAugmentID::ThornArmor, UThornArmorSkill::StaticClass());
    SkillClassTable.Add(EAugmentID::Vampire, UVampireSkill::StaticClass());
    SkillClassTable.Add(EAugmentID::Regeneration, URegenerationSkill::StaticClass());

    //액티브
    SkillClassTable.Add(EAugmentID::Knockback, UKnockbackSkill::StaticClass());
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

//증강 이름 보상 UI와 보유 증강 목록에서 씀
FText UDispatchTableComponent::GetAugmentDisplayName(EAugmentID AugmentID)
{
    switch (AugmentID)
    {
    case EAugmentID::AttackUp:
        return NSLOCTEXT("Augment", "AttackUpName", "공격력 증가");
    case EAugmentID::DefenceUp:
        return NSLOCTEXT("Augment", "DefenceUpName", "방어력 증가");
    case EAugmentID::HealthUp:
        return NSLOCTEXT("Augment", "HealthUpName", "최대 체력 증가");
    case EAugmentID::StaminaUp:
        return NSLOCTEXT("Augment", "StaminaUpName", "최대 스태미나 증가");
    case EAugmentID::Berserker:
        return NSLOCTEXT("Augment", "BerserkerName", "광전사");
    case EAugmentID::LastFortress:
        return NSLOCTEXT("Augment", "LastFortressName", "최후의 요새");
    case EAugmentID::ThornArmor:
        return NSLOCTEXT("Augment", "ThornArmorName", "가시 갑옷");
    case EAugmentID::Vampire:
        return NSLOCTEXT("Augment", "VampireName", "흡혈");
    case EAugmentID::Regeneration:
        return NSLOCTEXT("Augment", "RegenerationName", "재생력");
    case EAugmentID::Knockback:
        return NSLOCTEXT("Augment", "KnockbackName", "충격탄");
    case EAugmentID::AreaAttack:
        return NSLOCTEXT("Augment", "AreaAttackName", "폭발탄");
    case EAugmentID::ContinuousAttack:
        return NSLOCTEXT("Augment", "ContinuousAttackName", "화염탄");
    default:
        return NSLOCTEXT("Augment", "UnknownName", "알 수 없는 증강");
    }
}

//증강 설명 수치는 AugmentTypes.h 값을 넣어서 밸런스를 바꾸면 설명도 같이 바뀜
FText UDispatchTableComponent::GetAugmentDescription(EAugmentID AugmentID)
{
    switch (AugmentID)
    {
    case EAugmentID::AttackUp:
        return FText::Format(
            NSLOCTEXT("Augment", "AttackUpDesc", "공격력이 {0} 늘어납니다. 여러 번 얻을 수 있습니다."),
            FText::AsNumber(ATTACK_POWER_UP_AMOUNT));
    case EAugmentID::DefenceUp:
        return FText::Format(
            NSLOCTEXT("Augment", "DefenceUpDesc", "방어력이 {0} 늘어납니다. 여러 번 얻을 수 있습니다."),
            FText::AsNumber(DEFENCE_POWER_UP_AMOUNT));
    case EAugmentID::HealthUp:
        return FText::Format(
            NSLOCTEXT("Augment", "HealthUpDesc", "최대 체력이 {0} 늘어나고 그만큼 회복합니다. 여러 번 얻을 수 있습니다."),
            FText::AsNumber(HEALTH_UP_AMOUNT));
    case EAugmentID::StaminaUp:
        return FText::Format(
            NSLOCTEXT("Augment", "StaminaUpDesc", "최대 스태미나가 {0} 늘어나고 그만큼 채워집니다. 여러 번 얻을 수 있습니다."),
            FText::AsNumber(STAMINA_UP_AMOUNT));
    case EAugmentID::Berserker:
        return FText::Format(
            NSLOCTEXT("Augment", "BerserkerDesc", "체력이 {0}% 이하일 때 공격력이 {1}배가 됩니다."),
            FText::AsNumber(FMath::RoundToInt(BERSERKER_THRESHOLD * 100.0f)),
            FText::AsNumber(BERSERKER_MULTIPLIER));
    case EAugmentID::LastFortress:
        return FText::Format(
            NSLOCTEXT("Augment", "LastFortressDesc", "체력이 {0}% 이하일 때 방어력이 {1}배가 됩니다."),
            FText::AsNumber(FMath::RoundToInt(LAST_FORTRESS_THRESHOLD * 100.0f)),
            FText::AsNumber(LAST_FORTRESS_MULTIPLIER));
    case EAugmentID::ThornArmor:
        return FText::Format(
            NSLOCTEXT("Augment", "ThornArmorDesc", "받은 피해의 {0}%를 공격자에게 되돌려줍니다."),
            FText::AsNumber(FMath::RoundToInt(THORN_ARMOR_REFLECT_RATIO * 100.0f)));
    case EAugmentID::Vampire:
        return FText::Format(
            NSLOCTEXT("Augment", "VampireDesc", "입힌 피해의 {0}%만큼 체력을 회복합니다."),
            FText::AsNumber(FMath::RoundToInt(VAMPIRE_HEAL_RATIO * 100.0f)));
    case EAugmentID::Regeneration:
        return FText::Format(
            NSLOCTEXT("Augment", "RegenerationDesc", "{0}초마다 체력을 {1} 회복합니다."),
            FText::AsNumber(REGENERATION_INTERVAL),
            FText::AsNumber(REGENERATION_HEAL_AMOUNT));
    case EAugmentID::Knockback:
        return NSLOCTEXT("Augment", "KnockbackDesc", "총에 맞은 적이 뒤로 밀려납니다.");
    case EAugmentID::AreaAttack:
        return FText::Format(
            NSLOCTEXT("Augment", "AreaAttackDesc", "총알이 맞은 지점 주변 {0}m 안의 적에게 피해의 {1}%가 함께 들어갑니다. 멀수록 약해집니다."),
            FText::AsNumber(AREA_ATTACK_RADIUS / 100.0f),
            FText::AsNumber(FMath::RoundToInt(AREA_ATTACK_DAMAGE_RATIO * 100.0f)));
    case EAugmentID::ContinuousAttack:
        return FText::Format(
            NSLOCTEXT("Augment", "ContinuousAttackDesc", "총에 맞은 적이 {0}초 동안 불타며 {1}초마다 피해의 {2}%를 입습니다. 방어력을 무시합니다."),
            FText::AsNumber(CONTINUOUS_ATTACK_DURATION),
            FText::AsNumber(CONTINUOUS_ATTACK_INTERVAL),
            FText::AsNumber(FMath::RoundToInt(CONTINUOUS_ATTACK_DAMAGE_RATIO * 100.0f)));
    default:
        return FText::GetEmpty();
    }
}

//풀에 남아 있는 증강 정보
bool UDispatchTableComponent::FindAugmentData(EAugmentID AugmentID, FAugmentData& OutAugmentData) const
{
    const FAugmentData* FoundAugmentData = AugmentPool.Find(AugmentID);

    if (!FoundAugmentData)
    {
        return false;
    }

    OutAugmentData = *FoundAugmentData;

    return true;
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

// 증강 연출
// 증강 스킬은 UObject라 월드에 이펙트를 직접 붙일 수 없어서 이 컴포넌트가 대신 재생해줌

//가시 반사를 맞은 대상 발밑에 가시를 냄
void UDispatchTableComponent::PlayThornReflectEffect(AActor* ReflectTarget)
{
    //가시는 반사를 맞은 쪽 발밑에서 솟음 대상이 없으면 어디에 낼지 알 수 없음
    if (!ReflectTarget || !ThornSpikeEffectClass)
    {
        return;
    }

    //캐릭터의 원점은 캡슐 한가운데라 그대로 두면 가시가 허리에서 솟음
    //캡슐 절반 높이만큼 내려서 발밑에 맞춤 캐릭터가 아니면 원점이 곧 바닥이라 0
    FVector FootOffset = FVector::ZeroVector;

    if (const ACharacter* TargetCharacter = Cast<ACharacter>(ReflectTarget))
    {
        FootOffset.Z = -TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    }

    //몸에 붙이지 않고 그 자리에 두는 이유 땅에서 솟는 가시라 맞고 밀려나도 자리에 남아야 함
    //수명은 액터가 스스로 정해서 지워짐
    GetWorld()->SpawnActor<AThornSpikeEffect>(
        ThornSpikeEffectClass,
        ReflectTarget->GetActorLocation() + FootOffset,
        FRotator::ZeroRotator
    );
}
