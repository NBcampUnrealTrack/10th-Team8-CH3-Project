#include "DispatchTableComponent.h"
#include "DreamAugmentCatalog.h"
#include "DreamAugmentRuntime.h"
#include "DreamAugmentFeedback.h"
#include "MainPlayerCharacter.h"
#include "MainPlayerController.h"
#include "CombatStatsComponent.h"
#include "MonsterBase.h"
#include "BossMonster.h"
#include "EliteMonster.h"
#include "MonsterAIController.h"
#include "MonsterSkill.h"
#include "AugmentDamageLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"


UDreamAugmentRuntime* UDispatchTableComponent::GetRuntime()
{
    if(!Cast<AMainPlayerCharacter>(GetOwner()))return nullptr;
    if(!CatalogRuntime)CatalogRuntime=NewObject<UDreamAugmentRuntime>(this);return CatalogRuntime;
}
EAugmentCategory UDispatchTableComponent::GetAugmentCategory(EAugmentID ID){const auto* D=DreamAugmentCatalog::Find(ID);return D?D->Category:EAugmentCategory::Active;}
bool UDispatchTableComponent::IsEvolution(EAugmentID ID){const auto* D=DreamAugmentCatalog::Find(ID);return D&&D->Evolution;}
void UDispatchTableComponent::RecordAcquisition(EAugmentID ID)
{
    AugmentHistory.Add(ID);
    int32& Count=StackCounts.FindOrAdd(ID);
    if(++Count==1)
    {
        if(IsEvolution(ID))
        {
            const auto* Definition=DreamAugmentCatalog::Find(ID);
            if(Definition&&!Definition->Permanent)SkillOrder.Add(ID);
        }
        else if(GetAugmentCategory(ID)==EAugmentCategory::Passive)++PassiveSpecies;
        else ++ActiveSpecies;
    }
    ++AugmentRevision;
    if(!bRestoringAugments)UDreamAugmentFeedback::Acquired(GetOwner(),ID);
}
int32 UDispatchTableComponent::GetStackCount(EAugmentID ID)const{return StackCounts.FindRef(ID);}
int32 UDispatchTableComponent::GetOwnedCount(EAugmentCategory Category)const{return Category==EAugmentCategory::Passive?PassiveSpecies:ActiveSpecies;}
TArray<EAugmentID> UDispatchTableComponent::GetSkillIDs()const{return SkillOrder;}
int32 UDispatchTableComponent::GetSkillCount()const{return SkillOrder.Num();}
EAugmentID UDispatchTableComponent::GetSkillAtSlot(int32 Slot)const{return SkillOrder.IsValidIndex(Slot)?SkillOrder[Slot]:static_cast<EAugmentID>(255);}
bool UDispatchTableComponent::CanEvolve(EAugmentID ID)const
{
    const auto* D=DreamAugmentCatalog::Find(ID);if(!HasBegunPlay()||!Cast<AMainPlayerCharacter>(GetOwner())||!StatsComponent||StatsComponent->IsDead()||!D||!D->Evolution||HasAcquiredAugment(ID))return false;
    for(auto Part:D->Ingredients)if(!HasAcquiredAugment(Part))return false;
    return D->Permanent||bRestoringAugments||GetSkillCount()<FMath::Clamp(SkillLimit,1,6);
}
bool UDispatchTableComponent::CanAcquireAugment(EAugmentID ID)const
{
    if(!HasBegunPlay()||(StatsComponent&&StatsComponent->IsDead()))return false;
    const auto* Def=DreamAugmentCatalog::Find(ID);if(!Def)return false;if(Def->Evolution)return CanEvolve(ID);
    const bool Passive=Def->Category==EAugmentCategory::Passive;if(!Passive&&HasAcquiredAugment(ID))return false;
    if(!Cast<AMainPlayerCharacter>(GetOwner()))return AugmentPool.Find(ID)!=nullptr;
    if(Passive)
    {
        int32 Cap=MAX_int32;
        switch(ID)
        {
        case EAugmentID::WalkSpeedUp:case EAugmentID::SprintSpeedUp:case EAugmentID::RecoilControl:case EAugmentID::RegenDelay:Cap=15;break;
        case EAugmentID::JumpUp:Cap=10;break;
        case EAugmentID::ArmorPierce:Cap=30;break;
        case EAugmentID::EffectPower:Cap=25;break;
        case EAugmentID::Weakpoint:case EAugmentID::EliteHunter:case EAugmentID::BossHunter:case EAugmentID::EffectRange:case EAugmentID::EffectDuration:case EAugmentID::CooldownMastery:case EAugmentID::SkillEfficiency:case EAugmentID::StaminaRegen:case EAugmentID::HealingEfficiency:case EAugmentID::Learning:case EAugmentID::Collector:Cap=20;break;
        default:break;
        }
        if(GetStackCount(ID)>=Cap)return false;
    }
    if(HasAcquiredAugment(ID))return Passive;return GetOwnedCount(Def->Category)<FMath::Clamp(Passive?PassiveLimit:ActiveLimit,1,6);
}
FAugmentPool UDispatchTableComponent::GetEligiblePool()const
{
    FAugmentPool Pool;for(const auto& D:AugmentPool.Augments)if(CanAcquireAugment(D.AugmentID)&&D.Weight>0)Pool.Augments.Add(D);
    for(const auto& D:DreamAugmentCatalog::All())if(D.Evolution&&CanEvolve(D.ID))Pool.Augments.Add(FAugmentData(D.ID,EAugmentCategory::Active,16.f,false));return Pool;
}
bool UDispatchTableComponent::DrawReplacementChoices(const TArray<EAugmentID>& Previous,TArray<EAugmentID>& Out)const
{
    FAugmentPool Fresh,Old;for(const auto& D:GetEligiblePool().Augments)(Previous.Contains(D.AugmentID)?Old:Fresh).Augments.Add(D);
    Fresh.DrawChoices(AUGMENT_CHOICE_COUNT,Out);if(Out.Num()<AUGMENT_CHOICE_COUNT){TArray<EAugmentID> Fallback;Old.DrawChoices(AUGMENT_CHOICE_COUNT-Out.Num(),Fallback);for(auto ID:Fallback)Out.AddUnique(ID);}return !Out.IsEmpty();
}
float UDispatchTableComponent::GetEffectRangeMultiplier()const{return 1+FMath::Min(.6f,GetStackCount(EAugmentID::EffectRange)*.03f);}
FString UDispatchTableComponent::GetEvolutionStatus(EAugmentID ID)const
{
    const auto* D=DreamAugmentCatalog::Find(ID);if(!D||!D->Evolution)return TEXT("진화 루트 없음");if(HasAcquiredAugment(ID))return TEXT("획득 완료");
    FString Missing;for(auto Part:D->Ingredients)if(!HasAcquiredAugment(Part)){if(!Missing.IsEmpty())Missing+=TEXT(" · ");Missing+=GetAugmentDisplayName(Part).ToString()+TEXT(" 0/1");}
    if(!Missing.IsEmpty())return TEXT("재료 부족: ")+Missing;
    if(!D->Permanent&&GetSkillCount()>=FMath::Clamp(SkillLimit,1,6))return TEXT("스킬 보유 한도 6/6");
    return TEXT("조건 충족 · 증강 선택에서 등장 가능");
}
bool UDispatchTableComponent::TryEvolve(EAugmentID ID)
{
    if(!CanEvolve(ID))return false;auto* P=Cast<AMainPlayerCharacter>(GetOwner());
    if(!bRestoringAugments&&(!P||!P->GetCurrentAugmentChoices().Contains(ID)))return false;
    RecordAcquisition(ID);
    if(ID==EAugmentID::LastBreath&&!bLastBreathApplied){bLastBreathApplied=true;StatsComponent->AddDefencePower(LastBreathDefenceBonus);StatsComponent->SetHealthCapRatio(FMath::Clamp(LastBreathHealthRatio,.01f,.2f));HandleCurrentHealthChanged(StatsComponent->GetCurrentHealth(),StatsComponent->GetCurrentHealth());}
    if(auto* R=GetRuntime()){R->Start();if(ID!=EAugmentID::LastBreath)R->UnlockSkill(ID);}return true;
}
float UDispatchTableComponent::GetSkillCooldownRemaining(int32 Slot)const{return CatalogRuntime?CatalogRuntime->CooldownRemaining(GetSkillAtSlot(Slot)):0;}
float UDispatchTableComponent::GetSkillCost(int32 Slot)const{return CatalogRuntime?CatalogRuntime->GetCost(GetSkillAtSlot(Slot)):0;}
float UDispatchTableComponent::GetSkillCooldown(int32 Slot)const{return CatalogRuntime?CatalogRuntime->GetCooldown(GetSkillAtSlot(Slot)):0;}
FString UDispatchTableComponent::GetSkillStateText(int32 Slot)const{return CatalogRuntime?CatalogRuntime->SkillState(GetSkillAtSlot(Slot)):TEXT("미해금");}
bool UDispatchTableComponent::TryUseSkill(int32 Slot)
{
    if(!SkillOrder.IsValidIndex(Slot)||!GetRuntime())return false;
    const bool Success=CatalogRuntime->Use(SkillOrder[Slot]);
    auto* P=Cast<AMainPlayerCharacter>(GetOwner());auto* PC=P?Cast<AMainPlayerController>(P->GetController()):nullptr;
    if(PC&&!UGameplayStatics::IsGamePaused(this)&&!PC->IsMenuWidgetOpen()&&!P->HasAugmentChoices())
        if(auto* Feedback=GetWorld()->GetSubsystem<UDreamAugmentFeedback>())Feedback->SkillResult(P,SkillOrder[Slot],Success);
    return Success;
}

void UCombatStatsComponent::SetHealthCapRatio(float Ratio)
{
    HealthCapRatio=FMath::Clamp(Ratio,.01f,1.f);
    // A cap never grants health or resurrects. Every heal/current setter shares the same limit.
    if (GetCurrentHealth()>GetCurrentHealthLimit()) SetCurrentHealth(GetCurrentHealthLimit());
}
bool AMainPlayerCharacter::CanRerollAugmentChoices() const
{
    if (bAugmentRerollUsed || !IsAugmentChoiceInputReady() || !DispatchTable || (CombatStats && CombatStats->IsDead())) return false;
    for (const auto& Data:DispatchTable->GetEligiblePool().Augments)
        if (Data.Weight>0 && DispatchTable->CanAcquireAugment(Data.AugmentID) && !CurrentAugmentChoices.Contains(Data.AugmentID)) return true;
    return false;
}
bool AMainPlayerCharacter::RerollAugmentChoices()
{
    if (!CanRerollAugmentChoices()) return false;
    TArray<EAugmentID> Replacement;
    if (!DispatchTable->DrawReplacementChoices(CurrentAugmentChoices,Replacement)) return false;
    bAugmentRerollUsed=true;
    CurrentAugmentChoices=MoveTemp(Replacement);
    // Do not consume a pending reward, grant an augment, or rebroadcast widget creation.
    // The open native toolbar refreshes the same Blueprint card fields in-place.
    AugmentChoiceUnlockTime=FPlatformTime::Seconds()+.35;
    return true;
}
bool AMainPlayerCharacter::TrySpendSkillStamina(float Amount)
{
    if (!GetWorld() || !FMath::IsFinite(Amount) || Amount<0 || UGameplayStatics::IsGamePaused(this)
        || (CombatStats && CombatStats->IsDead()) || CurrentStamina-Amount<20.f) return false;
    SetCurrentStamina(CurrentStamina-Amount);
    LastStaminaUseTime=GetWorld()->GetTimeSeconds();
    if (!GetWorldTimerManager().IsTimerActive(StaminaTimerHandle))
        GetWorldTimerManager().SetTimer(StaminaTimerHandle,this,&AMainPlayerCharacter::UpdateStamina,.05f,true);
    return true;
}

bool AMonsterBase::TryApplyAugmentStagger(float Duration,float Immunity)
{
    if (!GetWorld() || !MonsterCombatStats || MonsterCombatStats->IsDead() || bIsRagdoll || bAugmentStaggered
        || Cast<ABossMonster>(this) || (MonsterSkill && MonsterSkill->IsUsingSkill())
        || !FMath::IsFinite(Duration) || Duration<=0 || GetWorld()->GetTimeSeconds()<NextAugmentStaggerTime) return false;
    if (Cast<AEliteMonster>(this)) Duration*=.5f;
    Duration=FMath::Clamp(Duration,.01f,1.f);
    bAugmentStaggered=true;
    NextAugmentStaggerTime=GetWorld()->GetTimeSeconds()+FMath::Max(Duration,Immunity);
    CancelAttack();HideAttackWarning();
    auto* Movement=GetCharacterMovement();
    PreStaggerMovementMode=Movement->MovementMode;
    Movement->StopMovementImmediately();Movement->DisableMovement();
    if (auto* AI=Cast<AMonsterAIController>(GetController())) { AI->StopMovement();AI->SetRagdollState(true); }
    GetWorldTimerManager().SetTimer(AugmentStaggerTimer,this,&AMonsterBase::FinishAugmentStagger,Duration,false);
    return true;
}
void AMonsterBase::FinishAugmentStagger()
{
    GetWorldTimerManager().ClearTimer(AugmentStaggerTimer);
    if (!bAugmentStaggered) return;
    bAugmentStaggered=false;
    if (!bIsRagdoll && MonsterCombatStats && !MonsterCombatStats->IsDead())
    {
        GetCharacterMovement()->SetMovementMode(EMovementMode(PreStaggerMovementMode));
        if (auto* AI=Cast<AMonsterAIController>(GetController())) AI->SetRagdollState(false);
    }
}


FText UDispatchTableComponent::GetCurrentAugmentDescription(EAugmentID ID) const
{
    if(HasAcquiredAugment(EAugmentID::LastBreath))
    {
        if(ID==EAugmentID::Berserker) return FText::FromString(FString::Printf(TEXT("체력 %.0f%% 이하: 공격력 %.1f배.\n체력 10%% 이하: 공격력 %.1f배.\n마지막 한숨 적용 중. 높은 배율만 적용됩니다."),LastBreathHealthRatio*100,LastBreathAttackMultiplier,LastBreathCriticalMultiplier));
        if(ID==EAugmentID::LastFortress) return FText::FromString(FString::Printf(TEXT("방어력 +%.0f 후 방어력 1.5배.\n마지막 한숨 적용 중: 체력·회복 상한 %.0f%%."),LastBreathDefenceBonus,LastBreathHealthRatio*100));
    }
    if(ID==EAugmentID::SuppressionPulse) return FText::FromString(FString::Printf(TEXT("주변 %.2fm 적을 %.2f초 경직시킵니다.\n기본 범위 %.2fm. 엘리트 지속 시간 절반.\n보스 면역·특수 패턴 보호. 피해·무적 없음.\n기본 스태미나 %.0f · 재사용 %.1f초\n사용 후 스태미나 20 이상 필요."),PulseRadius*GetEffectRangeMultiplier()/100,PulseStaggerDuration,PulseRadius/100,PulseStaminaCost,PulseCooldown));
    if(ID==EAugmentID::LastBreath) return FText::FromString(FString::Printf(TEXT("현재 체력·회복 상한: 최대 체력의 %.0f%%.\n공격력 %.1f배, 체력 10%% 이하 %.1f배.\n방어력 +%.0f 후 최후의 요새 1.5배 적용.\n영구 진화 · 해제 불가. 진화 시 회복하지 않습니다.\n버튼·스태미나·스킬 칸을 사용하지 않습니다."),LastBreathHealthRatio*100,LastBreathAttackMultiplier,LastBreathCriticalMultiplier,LastBreathDefenceBonus));
    const auto* Definition=DreamAugmentCatalog::Find(ID);
    FString Description=Definition?Definition->Description:GetAugmentDescription(ID).ToString();
    if(const auto* Def=DreamAugmentCatalog::Find(ID);Def&&Def->Evolution&&!Def->Permanent)
        Description+=TEXT("\n사용 후 스태미나 20 이상 필요.");
    return FText::FromString(Description);
}

EAugmentID UDispatchTableComponent::FindAugmentByCode(const FString& Code){return DreamAugmentCatalog::FromCode(Code);}
