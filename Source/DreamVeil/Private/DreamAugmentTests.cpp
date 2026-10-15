#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "MainPlayerCharacter.h"
#include "MainPlayerController.h"
#include "DispatchTableComponent.h"
#include "CombatStatsComponent.h"
#include "MonsterBase.h"
#include "BossMonster.h"
#include "EliteMonster.h"
#include "MonsterSkill.h"
#include "DreamVeilGameInstance.h"
#include "DreamVeilSaveGame.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UnrealType.h"
#include "HAL/PlatformProcess.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

namespace DreamAugmentTests
{
struct FWorld
{
    UWorld* World;
    FWorld()
    {
        auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());World->BeginPlay();World->GetWorldSettings()->NotifyBeginPlay();
    }
    ~FWorld(){GEngine->DestroyWorldContext(World);World->DestroyWorld(false);}
    void Advance(float Seconds){for(float T=0;T<Seconds;T+=.01f){++GFrameCounter;World->Tick(LEVELTICK_All,.01f);}}
};
void SetChoices(AMainPlayerCharacter* P,const TArray<EAugmentID>& Choices)
{
    auto* Prop=FindFProperty<FArrayProperty>(P->GetClass(),TEXT("CurrentAugmentChoices"));
    *Prop->ContainerPtrToValuePtr<TArray<EAugmentID>>(P)=Choices;
}
}
#define AUGMENT_TEST(Class,Name) IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class,"DreamVeil.Augment." Name,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

AUGMENT_TEST(FDreamAugmentLimits,"LimitsAndCategories")
bool FDreamAugmentLimits::RunTest(const FString&)
{
    DreamAugmentTests::FWorld W;auto* P=W.World->SpawnActor<AMainPlayerCharacter>();auto* D=P->DispatchTable.Get();
    TestEqual(TEXT("Passive cap"),D->PassiveLimit,6);TestEqual(TEXT("Active cap"),D->ActiveLimit,6);TestEqual(TEXT("Skill cap"),D->SkillLimit,6);
    TestTrue(TEXT("Stamina stat is passive"),D->GetAugmentCategory(EAugmentID::StaminaUp)==EAugmentCategory::Passive);
    TestTrue(TEXT("Berserker is automatic active"),D->GetAugmentCategory(EAugmentID::Berserker)==EAugmentCategory::Active);
    for(auto ID:{EAugmentID::Berserker,EAugmentID::LastFortress,EAugmentID::ThornArmor,EAugmentID::Vampire,EAugmentID::Regeneration,EAugmentID::Knockback})TestTrue(TEXT("Six distinct actives accepted"),D->ApplyAugment(ID));
    TestEqual(TEXT("Six active species"),D->GetOwnedCount(EAugmentCategory::Active),6);
    TestFalse(TEXT("Seventh active rejected"),D->ApplyAugment(EAugmentID::AreaAttack));
    TestFalse(TEXT("Duplicate unique active rejected"),D->ApplyAugment(EAugmentID::Berserker));
    TestTrue(TEXT("Passive still accepted"),D->ApplyAugment(EAugmentID::AttackUp));
    for(int32 I=0;I<20;++I)TestTrue(TEXT("Passive stacks keep one slot"),D->ApplyAugment(EAugmentID::AttackUp));
    TestEqual(TEXT("One passive species"),D->GetOwnedCount(EAugmentCategory::Passive),1);TestEqual(TEXT("Twenty-one stacks"),D->GetStackCount(EAugmentID::AttackUp),21);
    TArray<EAugmentID> Out;
    for(int32 I=0;I<40;++I){D->DrawAugmentChoices(Out);TSet<EAugmentID> Unique(Out);TestEqual(TEXT("Unique choices"),Unique.Num(),Out.Num());for(auto ID:Out)TestTrue(TEXT("No active offer after active cap"),D->GetAugmentCategory(ID)==EAugmentCategory::Passive||D->IsEvolution(ID));}
    D->PassiveLimit=1;TestFalse(TEXT("New passive blocked at test limit"),D->ApplyAugment(EAugmentID::HealthUp));TestTrue(TEXT("Existing passive can still stack"),D->ApplyAugment(EAugmentID::AttackUp));
    return true;
}
AUGMENT_TEST(FDreamAugmentReroll,"RerollOneFreePerOffer")
bool FDreamAugmentReroll::RunTest(const FString&)
{
    DreamAugmentTests::FWorld W;auto* P=W.World->SpawnActor<AMainPlayerCharacter>();auto* D=P->DispatchTable.Get();
    DreamAugmentTests::SetChoices(P,{EAugmentID::AttackUp,EAugmentID::DefenceUp,EAugmentID::HealthUp});
    const auto Old=P->GetCurrentAugmentChoices();const int32 Pending=P->GetPendingAugmentChoiceCount();
    TestTrue(TEXT("Initial reroll allowed"),P->CanRerollAugmentChoices());TestTrue(TEXT("Reroll succeeds"),P->RerollAugmentChoices());
    TestEqual(TEXT("Reroll consumed once"),P->GetAugmentRerollsRemaining(),0);TestEqual(TEXT("Pending rewards unchanged"),P->GetPendingAugmentChoiceCount(),Pending);
    TestEqual(TEXT("No augment granted"),D->GetAugmentHistory().Num(),0);
    const auto Fresh=P->GetCurrentAugmentChoices();TestEqual(TEXT("Three new choices"),Fresh.Num(),3);for(auto ID:Fresh)TestFalse(TEXT("Old choices excluded when alternatives suffice"),Old.Contains(ID));
    FPlatformProcess::Sleep(.37f);TestFalse(TEXT("Second reroll rejected after debounce"),P->RerollAugmentChoices());
    TestFalse(TEXT("Old stale selection rejected"),P->SelectAugmentChoice(Old[0]));TestEqual(TEXT("Stale selection does not grant"),D->GetAugmentHistory().Num(),0);
    return true;
}
AUGMENT_TEST(FDreamAugmentEmptyReroll,"RerollNoNewCandidateKeepsCharge")
bool FDreamAugmentEmptyReroll::RunTest(const FString&)
{
    DreamAugmentTests::FWorld W;auto* P=W.World->SpawnActor<AMainPlayerCharacter>();auto* D=P->DispatchTable.Get();D->AugmentPool.Augments.Empty();
    for(auto ID:{EAugmentID::AttackUp,EAugmentID::DefenceUp,EAugmentID::HealthUp})D->AugmentPool.Augments.Add(FAugmentData(ID,EAugmentCategory::Passive,1,true));
    DreamAugmentTests::SetChoices(P,{EAugmentID::AttackUp,EAugmentID::DefenceUp,EAugmentID::HealthUp});
    TestFalse(TEXT("No alternative"),P->CanRerollAugmentChoices());TestFalse(TEXT("Reroll does not pretend success"),P->RerollAugmentChoices());TestEqual(TEXT("Free charge retained"),P->GetAugmentRerollsRemaining(),1);
    D->AugmentPool.Augments.Add(FAugmentData(EAugmentID::StaminaUp,EAugmentCategory::Passive,1,true));
    TestTrue(TEXT("One alternative sufficient"),P->RerollAugmentChoices());TestTrue(TEXT("New alternative included"),P->GetCurrentAugmentChoices().Contains(EAugmentID::StaminaUp));TestEqual(TEXT("Fallback fills to three"),P->GetCurrentAugmentChoices().Num(),3);return true;
}
AUGMENT_TEST(FDreamLastBreath,"LastBreathCapAndRestore")
bool FDreamLastBreath::RunTest(const FString&)
{
    DreamAugmentTests::FWorld W;auto* P=W.World->SpawnActor<AMainPlayerCharacter>();auto* D=P->DispatchTable.Get();auto* S=P->CombatStats.Get();S->InitStats(100,0,10);
    TestFalse(TEXT("Missing ingredients cannot evolve"),D->TryEvolve(EAugmentID::LastBreath));D->ApplyAugment(EAugmentID::Berserker);D->ApplyAugment(EAugmentID::LastFortress);
    TestFalse(TEXT("No free codex evolution"),D->TryEvolve(EAugmentID::LastBreath));DreamAugmentTests::SetChoices(P,{EAugmentID::LastBreath});TestTrue(TEXT("Ready evolves from reward"),P->SelectAugmentChoice(EAugmentID::LastBreath));TestEqual(TEXT("Maximum health is real maximum"),S->GetMaxHealth(),100.f);TestEqual(TEXT("Current health capped"),S->GetCurrentHealth(),20.f);
    TestTrue(TEXT("Enhanced attack tier"),FMath::IsNearlyEqual(S->GetAttackPower(),18.f));TestTrue(TEXT("Flat defence before fortress multiplier"),FMath::IsNearlyEqual(S->GetDefencePower(),15.f));
    S->Heal(999);S->SetCurrentHealth(999);TestEqual(TEXT("All heal/current setters obey cap"),S->GetCurrentHealth(),20.f);
    S->SetCurrentHealth(10);TestTrue(TEXT("Critical tier replaces normal tier"),FMath::IsNearlyEqual(S->GetAttackPower(),23.f));
    D->ApplyAugment(EAugmentID::HealthUp);TestEqual(TEXT("Health augment keeps actual max"),S->GetMaxHealth(),120.f);TestEqual(TEXT("Health augment obeys cap"),S->GetCurrentHealth(),24.f);
    TestFalse(TEXT("Duplicate evolution blocked"),D->TryEvolve(EAugmentID::LastBreath));TestEqual(TEXT("Defence not duplicated"),S->GetDefencePower(),15.f);TestEqual(TEXT("Permanent effect no button slot"),D->GetSkillCount(),0);
    const auto History=D->GetAugmentHistory();auto* P2=W.World->SpawnActor<AMainPlayerCharacter>(FVector(0,0,10000),FRotator::ZeroRotator);P2->CombatStats->InitStats(100,0,10);
    TestTrue(TEXT("History restore includes evolution"),P2->DispatchTable->RestoreAugments(History));TestTrue(TEXT("Restored evolved state"),P2->DispatchTable->HasAcquiredAugment(EAugmentID::LastBreath));
    TestEqual(TEXT("Restored cap"),P2->CombatStats->GetCurrentHealthLimit(),24.f);TestEqual(TEXT("Restored defence once"),P2->CombatStats->GetDefencePower(),15.f);
    TestFalse(TEXT("Duplicate restore refused"),P2->DispatchTable->RestoreAugments(History));
    return true;
}
AUGMENT_TEST(FDreamLastBreathLow,"LastBreathNeverHealsAndDeadStaysDead")
bool FDreamLastBreathLow::RunTest(const FString&)
{
    DreamAugmentTests::FWorld W;auto* P=W.World->SpawnActor<AMainPlayerCharacter>();auto* D=P->DispatchTable.Get();auto* S=P->CombatStats.Get();S->InitStats(100,0,10);S->SetCurrentHealth(5);
    D->ApplyAugment(EAugmentID::Berserker);D->ApplyAugment(EAugmentID::LastFortress);DreamAugmentTests::SetChoices(P,{EAugmentID::LastBreath});P->SelectAugmentChoice(EAugmentID::LastBreath);
    TestEqual(TEXT("Evolution does not heal injured player"),S->GetCurrentHealth(),5.f);S->SetMaxHealth(200);TestEqual(TEXT("Max increase does not heal"),S->GetCurrentHealth(),5.f);
    S->ApplyIncomingDamage(999);S->Heal(999);TestTrue(TEXT("Healing does not resurrect"),S->IsDead());TestEqual(TEXT("Dead health remains zero"),S->GetCurrentHealth(),0.f);return true;
}
AUGMENT_TEST(FDreamStagger,"StaggerDurationImmunityPoolAndBoss")
bool FDreamStagger::RunTest(const FString&)
{
    DreamAugmentTests::FWorld W;auto* M=W.World->SpawnActor<AMonsterBase>();
    TestTrue(TEXT("Normal enemy staggers"),M->TryApplyAugmentStagger(.2f));TestEqual(TEXT("Movement disabled"),M->GetCharacterMovement()->MovementMode.GetValue(),MOVE_None);
    TestFalse(TEXT("No repeated stagger extension"),M->TryApplyAugmentStagger(.5f));W.Advance(.22f);TestFalse(TEXT("Stagger expires"),M->IsAugmentStaggered());TestTrue(TEXT("Movement restored"),M->GetCharacterMovement()->MovementMode!=MOVE_None);
    TestFalse(TEXT("Immunity prevents stunlock"),M->TryApplyAugmentStagger(.2f));W.Advance(1.4f);TestTrue(TEXT("Available after immunity"),M->TryApplyAugmentStagger(.2f));
    M->OnPoolReturned();TestFalse(TEXT("Pool clears stagger"),M->IsAugmentStaggered());M->PrepareForPoolReuse();M->OnPoolAcquired();TestTrue(TEXT("Reacquired enemy can stagger"),M->TryApplyAugmentStagger(.2f));
    auto* B=W.World->SpawnActor<ABossMonster>(FVector(20000,0,10000),FRotator::ZeroRotator);TestFalse(TEXT("Boss immune"),B->TryApplyAugmentStagger(.5f));
    auto* E=W.World->SpawnActor<AEliteMonster>(FVector(30000,0,10000),FRotator::ZeroRotator);TestTrue(TEXT("Elite affected"),E->TryApplyAugmentStagger(.4f));W.Advance(.22f);TestFalse(TEXT("Elite half duration"),E->IsAugmentStaggered());return true;
}
AUGMENT_TEST(FDreamPulse,"PulseRequirementsCooldownStaminaAndPause")
bool FDreamPulse::RunTest(const FString&)
{
    DreamAugmentTests::FWorld W;auto* P=W.World->SpawnActor<AMainPlayerCharacter>();auto* PC=W.World->SpawnActor<AMainPlayerController>();PC->Possess(P);auto* D=P->DispatchTable.Get();
    TestFalse(TEXT("No skill before evolution"),D->TryUseSkill(0));
    for(auto ID:{EAugmentID::Knockback,EAugmentID::DefenceUp,EAugmentID::EffectRange})D->ApplyAugment(ID);
    DreamAugmentTests::SetChoices(P,{EAugmentID::SuppressionPulse});TestTrue(TEXT("Recipe evolves from reward"),P->SelectAugmentChoice(EAugmentID::SuppressionPulse));TestEqual(TEXT("One skill slot"),D->GetSkillCount(),1);TestEqual(TEXT("Ingredients kept"),D->GetOwnedCount(EAugmentCategory::Active),1);
    TestFalse(TEXT("No immediate travel or evolution cast"),D->TryUseSkill(0));W.Advance(10.1f);
    const float Before=P->GetCurrentStamina();TestTrue(TEXT("Ready skill casts"),D->TryUseSkill(0));TestTrue(TEXT("One stamina charge"),FMath::IsNearlyEqual(P->GetCurrentStamina(),Before-30));TestFalse(TEXT("Repeat cast blocked"),D->TryUseSkill(0));
    TestFalse(TEXT("Empty slot does not cast"),D->TryUseSkill(1));const float Remaining=D->GetSkillCooldownRemaining(0);TestTrue(TEXT("Cooldown starts"),Remaining>9.9f);
    W.Advance(10.1f);TestTrue(TEXT("Cooldown expires"),D->GetSkillCooldownRemaining(0)==0);
    TestTrue(TEXT("Can spend while keeping escape reserve"),P->TrySpendSkillStamina(80));TestFalse(TEXT("Below reserve cannot cast"),D->TryUseSkill(0));TestEqual(TEXT("Failure spends nothing"),P->GetCurrentStamina(),20.f);
    TestTrue(TEXT("Range passive works"),FMath::IsNearlyEqual(D->GetEffectRangeMultiplier(),1.03f));for(int32 I=0;I<30;++I)D->ApplyAugment(EAugmentID::EffectRange);TestTrue(TEXT("Range has safety cap"),FMath::IsNearlyEqual(D->GetEffectRangeMultiplier(),1.6f,.0001f));
    return true;
}
#endif
