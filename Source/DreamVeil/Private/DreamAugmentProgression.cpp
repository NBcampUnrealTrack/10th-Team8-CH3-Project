#include "DispatchTableComponent.h"
#include "DreamAugmentCatalog.h"
#include "DreamAugmentRuntime.h"
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
int32 UDispatchTableComponent::GetStackCount(EAugmentID ID)const{int32 Count=0;for(auto V:AugmentHistory)if(V==ID)++Count;return Count;}
int32 UDispatchTableComponent::GetOwnedCount(EAugmentCategory Category)const{TSet<EAugmentID> Unique;for(auto V:AugmentHistory)if(!IsEvolution(V)&&GetAugmentCategory(V)==Category)Unique.Add(V);return Unique.Num();}
TArray<EAugmentID> UDispatchTableComponent::GetSkillIDs()const{TArray<EAugmentID> Out;for(auto V:AugmentHistory)if(const auto* D=DreamAugmentCatalog::Find(V);D&&D->Evolution&&!D->Permanent)Out.AddUnique(V);return Out;}
int32 UDispatchTableComponent::GetSkillCount()const{return GetSkillIDs().Num();}
EAugmentID UDispatchTableComponent::GetSkillAtSlot(int32 Slot)const{auto IDs=GetSkillIDs();return IDs.IsValidIndex(Slot)?IDs[Slot]:static_cast<EAugmentID>(255);}
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
    AugmentHistory.Add(ID);
    if(ID==EAugmentID::LastBreath&&!bLastBreathApplied){bLastBreathApplied=true;StatsComponent->AddDefencePower(LastBreathDefenceBonus);StatsComponent->SetHealthCapRatio(FMath::Clamp(LastBreathHealthRatio,.01f,.2f));HandleCurrentHealthChanged(StatsComponent->GetCurrentHealth(),StatsComponent->GetCurrentHealth());}
    if(auto* R=GetRuntime()){R->Start();if(ID!=EAugmentID::LastBreath)R->UnlockSkill(ID);}return true;
}
float UDispatchTableComponent::GetSkillCooldownRemaining(int32 Slot)const{return CatalogRuntime?CatalogRuntime->CooldownRemaining(GetSkillAtSlot(Slot)):0;}
float UDispatchTableComponent::GetSkillCost(int32 Slot)const{return CatalogRuntime?CatalogRuntime->GetCost(GetSkillAtSlot(Slot)):0;}
float UDispatchTableComponent::GetSkillCooldown(int32 Slot)const{return CatalogRuntime?CatalogRuntime->GetCooldown(GetSkillAtSlot(Slot)):0;}
FString UDispatchTableComponent::GetSkillStateText(int32 Slot)const{return CatalogRuntime?CatalogRuntime->SkillState(GetSkillAtSlot(Slot)):TEXT("미해금");}
bool UDispatchTableComponent::TryUseSkill(int32 Slot){const auto IDs=GetSkillIDs();return IDs.IsValidIndex(Slot)&&GetRuntime()&&CatalogRuntime->Use(IDs[Slot]);}

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
        if(ID==EAugmentID::Berserker) return FText::FromString(FString::Printf(TEXT("마지막 한숨 적용: 체력 %.0f%% 이하에서 공격력 %.1f배, 10%% 이하에서 %.1f배입니다. 두 배율은 중첩되지 않습니다."),LastBreathHealthRatio*100,LastBreathAttackMultiplier,LastBreathCriticalMultiplier));
        if(ID==EAugmentID::LastFortress) return FText::FromString(FString::Printf(TEXT("마지막 한숨 적용: 고정 방어력 +%.0f 후 최후의 요새 1.5배가 적용됩니다. 실제 최대 체력은 유지하고 회복 상한은 %.0f%%입니다."),LastBreathDefenceBonus,LastBreathHealthRatio*100));
        if(ID==EAugmentID::HealthUp || ID==EAugmentID::Vampire || ID==EAugmentID::Regeneration)
            return FText::FromString(GetAugmentDescription(ID).ToString()+FString::Printf(TEXT(" 마지막 한숨의 %.0f%% 회복 상한을 넘지 않습니다."),LastBreathHealthRatio*100));
    }
    if(ID==EAugmentID::SuppressionPulse) return FText::FromString(FString::Printf(TEXT("경직탄 + 방어력 증가 1중첩 + 효과 범위 증가 1중첩. 재료와 기본 경직탄을 유지합니다. 기본 %.2fm (현재 %.2fm) 안의 일반 적을 %.2f초 경직. 엘리트 절반·보스 면역·특수 패턴 보호. 피해·무적 없음. 스태미나 %.0f + 잔여 20 필요, 재사용 %.1f초."),PulseRadius/100,PulseRadius*GetEffectRangeMultiplier()/100,PulseStaggerDuration,PulseStaminaCost,PulseCooldown));
    if(ID==EAugmentID::LastBreath) return FText::FromString(FString::Printf(TEXT("광전사 + 최후의 요새. 실제 최대 체력 유지, 현재 체력·모든 회복 상한 %.0f%%. 진화 시 회복하지 않습니다. 공격력 %.1f배 (체력 10%% 이하 %.1f배), 고정 방어력 +%.0f 후 요새 1.5배. 영구 효과이며 버튼·스태미나 소모·스킬 슬롯 사용이 없습니다."),LastBreathHealthRatio*100,LastBreathAttackMultiplier,LastBreathCriticalMultiplier,LastBreathDefenceBonus));
    return GetAugmentDescription(ID);
}

EAugmentID UDispatchTableComponent::FindAugmentByCode(const FString& Code){return DreamAugmentCatalog::FromCode(Code);}
