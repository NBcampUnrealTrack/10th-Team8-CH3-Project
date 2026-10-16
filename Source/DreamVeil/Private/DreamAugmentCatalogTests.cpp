#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DreamAugmentCatalog.h"
#include "DreamAugmentRuntime.h"
#include "AugmentDamageLibrary.h"
#include "MainPlayerCharacter.h"
#include "MainPlayerController.h"
#include "DispatchTableComponent.h"
#include "CombatStatsComponent.h"
#include "MonsterBase.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UnrealType.h"
#include "Kismet/GameplayStatics.h"
namespace DreamCatalogTests {
struct FWorld {
 UWorld* W;
 FWorld(){auto V=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);W=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&V);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);W->SetGameInstance(NewObject<UGameInstance>(GEngine));W->SetGameMode(FURL(nullptr,TEXT("?game=/Script/Engine.GameModeBase"),TRAVEL_Absolute));W->InitializeActorsForPlay(FURL());W->BeginPlay();W->GetWorldSettings()->NotifyBeginPlay();auto* Floor=W->SpawnActor<AStaticMeshActor>(FVector(0,0,-100),FRotator::ZeroRotator);Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));Floor->SetActorScale3D(FVector(100,100,.2));Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));}
 ~FWorld(){GEngine->DestroyWorldContext(W);W->DestroyWorld(false);}
 void Advance(float S){for(float T=0;T<S;T+=.01f){++GFrameCounter;W->Tick(LEVELTICK_All,.01f);}}
 AMainPlayerCharacter* Player(){auto* P=W->SpawnActor<AMainPlayerCharacter>();W->SpawnActor<AMainPlayerController>()->Possess(P);P->CombatStats->InitStats(100,0,10);return P;}
};
void Choices(AMainPlayerCharacter* P,TArray<EAugmentID> A){*FindFProperty<FArrayProperty>(P->GetClass(),TEXT("CurrentAugmentChoices"))->ContainerPtrToValuePtr<TArray<EAugmentID>>(P)=A;}
}
#define CAT_TEST(C,N) IMPLEMENT_SIMPLE_AUTOMATION_TEST(C,"DreamVeil.Catalog." N,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
CAT_TEST(FDreamCatalogCoverage,"All84DefinitionsAndStableIDs")
bool FDreamCatalogCoverage::RunTest(const FString&){int32 P=0,A=0,K=0,Last=0;TSet<EAugmentID> IDs;TSet<FString> Codes;for(const auto& D:DreamAugmentCatalog::All()){TestFalse(TEXT("Unique ID"),IDs.Contains(D.ID));IDs.Add(D.ID);TestFalse(TEXT("Unique code"),Codes.Contains(D.Code));Codes.Add(D.Code);TestFalse(TEXT("Name"),D.Name.IsEmpty());TestFalse(TEXT("Description"),D.Description.IsEmpty());if(D.Permanent)++Last;else if(D.Evolution)++K;else if(D.Category==EAugmentCategory::Passive)++P;else ++A;for(auto I:D.Ingredients){auto* Part=DreamAugmentCatalog::Find(I);TestTrue(TEXT("Recipe references base entry"),Part&&!Part->Evolution);}if(D.Evolution&&!D.Permanent){TestEqual(TEXT("Three ingredients"),D.Ingredients.Num(),3);TestTrue(TEXT("Positive cost/cooldown"),D.Cost>0&&D.Cooldown>0);}}TestEqual(TEXT("Passive 22"),P,22);TestEqual(TEXT("Auto active 31"),A,31);TestEqual(TEXT("Button skills 30"),K,30);TestEqual(TEXT("Permanent evolution"),Last,1);TestEqual(TEXT("Total"),IDs.Num(),84);TestEqual(TEXT("Legacy Attack numeric"),int32(EAugmentID::AttackUp),0);TestEqual(TEXT("Legacy LastBreath numeric"),int32(EAugmentID::LastBreath),13);TestEqual(TEXT("Legacy Pulse numeric"),int32(EAugmentID::SuppressionPulse),14);TestFalse(TEXT("Excluded P09"),Codes.Contains(TEXT("P09")));TestFalse(TEXT("Excluded P10"),Codes.Contains(TEXT("P10")));return true;}
CAT_TEST(FDreamAllBase,"All53BaseEntriesAcquireAndReset")
bool FDreamAllBase::RunTest(const FString&){DreamCatalogTests::FWorld W;int32 Count=0;for(const auto& Def:DreamAugmentCatalog::All()){if(Def.Evolution)continue;auto* P=W.Player();auto* D=P->DispatchTable.Get();TestTrue(*Def.Code,D->ApplyAugment(Def.ID));TestEqual(TEXT("Correct category slot"),D->GetOwnedCount(Def.Category),1);TestEqual(TEXT("One stack"),D->GetStackCount(Def.ID),1);TestEqual(TEXT("Only passive repeats"),D->ApplyAugment(Def.ID),Def.Category==EAugmentCategory::Passive);P->Destroy();++Count;}TestEqual(TEXT("All base entries"),Count,53);return true;}
CAT_TEST(FDreamAllRecipes,"All31RecipesRewardGateAndSaveRestore")
bool FDreamAllRecipes::RunTest(const FString&){DreamCatalogTests::FWorld W;int32 Count=0;for(const auto& Def:DreamAugmentCatalog::All()){if(!Def.Evolution)continue;auto* P=W.Player();auto* D=P->DispatchTable.Get();TestFalse(TEXT("Not ready without ingredients"),D->CanEvolve(Def.ID));for(auto I:Def.Ingredients)TestTrue(*Def.Code,D->ApplyAugment(I));TestTrue(*Def.Code,D->CanEvolve(Def.ID));TestFalse(TEXT("Codex cannot grant free evolution"),D->TryEvolve(Def.ID));DreamCatalogTests::Choices(P,{Def.ID});TestTrue(TEXT("Reward selection grants evolution"),P->SelectAugmentChoice(Def.ID));TestTrue(TEXT("History includes evolution"),D->HasAcquiredAugment(Def.ID));TestEqual(TEXT("Button slots"),D->GetSkillCount(),Def.Permanent?0:1);TestFalse(TEXT("Unique evolution"),D->TryEvolve(Def.ID));auto H=D->GetAugmentHistory();P->Destroy();auto* Copy=W.Player();TestTrue(TEXT("Restore catalog history"),Copy->DispatchTable->RestoreAugments(H));TestTrue(TEXT("Restored ID"),Copy->DispatchTable->HasAcquiredAugment(Def.ID));Copy->Destroy();++Count;}TestEqual(TEXT("Every recipe"),Count,31);return true;}
CAT_TEST(FDreamSkillCasts,"All30ButtonSkillsCostCooldownAndReserve")
bool FDreamSkillCasts::RunTest(const FString&){DreamCatalogTests::FWorld W;int32 Count=0;for(const auto& Def:DreamAugmentCatalog::All()){if(!Def.Evolution||Def.Permanent)continue;auto* P=W.Player();auto* D=P->DispatchTable.Get();for(auto I:Def.Ingredients)D->ApplyAugment(I);DreamCatalogTests::Choices(P,{Def.ID});P->SelectAugmentChoice(Def.ID);TestFalse(TEXT("Evolution does not grant instant cast"),D->TryUseSkill(0));W.Advance(D->GetSkillCooldown(0)+.15f);P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);const float Before=P->GetCurrentStamina(),Cost=D->GetSkillCost(0);if(Def.ID==EAugmentID::SkillDodge){FHitResult H;FCollisionQueryParams Q;Q.AddIgnoredActor(P);bool Ground=W.W->LineTraceSingleByChannel(H,P->GetActorLocation()+FVector(200,0,0),P->GetActorLocation()+FVector(200,0,-250),ECC_Visibility,Q);AddInfo(FString::Printf(TEXT("Dodge before: %s floor %d %s mode %d"),*P->GetActorLocation().ToString(),Ground,*GetNameSafe(H.GetActor()),int32(P->GetCharacterMovement()->MovementMode)));}bool Used=D->TryUseSkill(0);TestTrue(*FString::Printf(TEXT("%s casts"),*Def.Code),Used);if(Def.ID==EAugmentID::SkillOvercharge){TestEqual(TEXT("Preparation alone is free"),P->GetCurrentStamina(),Before);D->GetRuntime()->BeginShot(P->GetActorLocation(),FVector::ForwardVector,10,3000,ECC_Visibility);D->GetRuntime()->EndShot();}TestTrue(*FString::Printf(TEXT("%s charges exactly once"),*Def.Code),FMath::IsNearlyEqual(P->GetCurrentStamina(),Before-Cost,.01f));TestTrue(*FString::Printf(TEXT("%s starts cooldown"),*Def.Code),D->GetSkillCooldownRemaining(0)>0);if(Def.ID!=EAugmentID::SkillReturn)TestFalse(TEXT("Cannot double cast"),D->TryUseSkill(0));TestFalse(TEXT("Empty slot cannot cast"),D->TryUseSkill(5));TestTrue(TEXT("Escape stamina reserve"),P->GetCurrentStamina()>=20);P->Destroy();++Count;}TestEqual(TEXT("Every button skill"),Count,30);return true;}
CAT_TEST(FDreamCatalogDefense,"ShieldSlowSealAndPoolCleanup")
bool FDreamCatalogDefense::RunTest(const FString&){DreamCatalogTests::FWorld W;auto* P=W.Player();auto* D=P->DispatchTable.Get();const auto* Shield=DreamAugmentCatalog::Find(DreamAugmentCatalog::FromCode(TEXT("K20")));for(auto I:Shield->Ingredients)D->ApplyAugment(I);DreamCatalogTests::Choices(P,{Shield->ID});P->SelectAugmentChoice(Shield->ID);W.Advance(D->GetSkillCooldown(0)+.15f);TestTrue(TEXT("Shield cast"),D->TryUseSkill(0));auto* R=D->GetRuntime();const float Amount=R->ShieldAmount();TestTrue(TEXT("Shield absorbs"),FMath::IsNearlyEqual(R->AbsorbDamage(Amount+5,false,false),5));TestEqual(TEXT("Shield emptied"),R->ShieldAmount(),0.f);W.Advance(.15f);TestFalse(TEXT("HUD effect expires after break"),R->IsActive(Shield->ID));auto* M=W.W->SpawnActor<AMonsterBase>(FVector(500,0,0),FRotator::ZeroRotator);const float Speed=M->GetCharacterMovement()->MaxWalkSpeed;auto* S=UDreamAugmentStatus::For(M);S->Slow(TEXT("a"),.1f,1);S->Slow(TEXT("b"),.2f,1);TestTrue(TEXT("Slows use strongest without multiplying"),FMath::IsNearlyEqual(M->GetCharacterMovement()->MaxWalkSpeed,Speed*.8f));S->Seal(2);TestTrue(TEXT("Sealed"),S->IsSealed());M->OnPoolReturned();TestFalse(TEXT("Pool clears seal"),S->IsSealed());TestTrue(TEXT("Pool restores speed"),FMath::IsNearlyEqual(M->GetCharacterMovement()->MaxWalkSpeed,Speed));auto* Decoy=W.W->SpawnActor<ADreamAugmentField>(FVector(500,50,0),FRotator::ZeroRotator);Decoy->Configure(P,2,300,3,0);TestTrue(TEXT("Normal enemy selects decoy"),ADreamAugmentField::FindDecoyFor(M,P)==Decoy);FDamageEvent DamageEvent;TestEqual(TEXT("Decoy health absorbs damage"),Decoy->TakeDamage(7,DamageEvent,M->GetController(),M),7.f);TestEqual(TEXT("Decoy remaining health"),Decoy->TakeDamage(20,DamageEvent,M->GetController(),M),13.f);TestTrue(TEXT("Expired decoy restores player target"),ADreamAugmentField::FindDecoyFor(M,P)==P);R->ShotNumber=99;R->Reset();TestEqual(TEXT("Runtime reset clears shot cycle"),R->ShotNumber,0);TestEqual(TEXT("Runtime reset clears shield"),R->ShieldAmount(),0.f);return true;}

CAT_TEST(FDreamCatalogEffects,"FocusFireBurnTimingAndNoProcRecursion")
bool FDreamCatalogEffects::RunTest(const FString&){DreamCatalogTests::FWorld W;auto* P=W.Player();auto* D=P->DispatchTable.Get();D->ApplyAugment(EAugmentID::FocusFire);auto* R=D->GetRuntime();auto* M=W.W->SpawnActor<AMonsterBase>(FVector(500,0,0),FRotator::ZeroRotator);M->MonsterCombatStats->InitStats(1000,0,1);FHitResult H;H.ImpactPoint=M->GetActorLocation();for(int32 I=0;I<3;++I){TestEqual(TEXT("First three hits unchanged"),R->ModifyDirectDamage(M,100,false),100.f);R->AfterDirectHit(M,100,false,H);}TestEqual(TEXT("Fourth focus hit"),R->ModifyDirectDamage(M,100,false),125.f);R->AfterDirectHit(M,100,false,H);TestEqual(TEXT("Cycle resets"),R->ModifyDirectDamage(M,100,false),100.f);W.Advance(.03f);AddInfo(FString::Printf(TEXT("Burn fixture: damaged %d player dead %d"),M->CanBeDamaged(),P->CombatStats->IsDead()));R->Burn(M,100,3);W.Advance(1.2f);AddInfo(FString::Printf(TEXT("Burn after 1.2: time %f HP %f fire %d"),R->Now(),M->MonsterCombatStats->GetCurrentHealth(),M->MonsterCombatStats->IsOnFire()));W.Advance(2.f);TestEqual(TEXT("Three burn ticks even with off-grid start"),M->MonsterCombatStats->GetCurrentHealth(),970.f);TestFalse(TEXT("Burn visual expires"),M->MonsterCombatStats->IsOnFire());const int32 Shots=R->ShotNumber;UGameplayStatics::ApplyDamage(M,10,P->GetController(),P,UAugmentSecondaryDamageType::StaticClass());TestEqual(TEXT("Secondary damage does not count as another shot"),R->ShotNumber,Shots);TestEqual(TEXT("Secondary damage does not advance focus hits"),R->ModifyDirectDamage(M,100,false),100.f);return true;}

CAT_TEST(FDreamHighHealthAugment,"HealthAugmentAbove100AndPermanentRecoveryCap")
bool FDreamHighHealthAugment::RunTest(const FString&)
{
    DreamCatalogTests::FWorld W;auto* P=W.Player();auto* D=P->DispatchTable.Get();auto* S=P->CombatStats.Get();
    P->RestoreLevelProgress(11,0);S->ResetHealth();
    TestEqual(TEXT("Level growth maximum exceeds 100"),S->GetMaxHealth(),120.f);
    TestTrue(TEXT("First health reward"),D->ApplyAugment(EAugmentID::HealthUp));
    TestEqual(TEXT("Augment grows restored maximum"),S->GetMaxHealth(),140.f);
    TestEqual(TEXT("Augment heals above 100"),S->GetCurrentHealth(),140.f);
    S->SetCurrentHealth(90);D->ApplyAugment(EAugmentID::HealthUp);
    TestEqual(TEXT("Injured reward grows maximum"),S->GetMaxHealth(),160.f);
    TestEqual(TEXT("Injured reward heals 20, not full refill"),S->GetCurrentHealth(),110.f);
    for(auto ID:{EAugmentID::Berserker,EAugmentID::LastFortress})D->ApplyAugment(ID);
    DreamCatalogTests::Choices(P,{EAugmentID::LastBreath});P->SelectAugmentChoice(EAugmentID::LastBreath);
    TestEqual(TEXT("Permanent cap is 20 percent"),S->GetCurrentHealth(),32.f);
    D->ApplyAugment(EAugmentID::HealthUp);
    TestEqual(TEXT("Permanent evolution keeps growing maximum"),S->GetMaxHealth(),180.f);
    TestEqual(TEXT("Health augment respects new recovery cap"),S->GetCurrentHealth(),36.f);
    S->SetCurrentHealth(5);S->ResetHealth();
    TestEqual(TEXT("Stage refill respects permanent recovery cap"),S->GetCurrentHealth(),36.f);
    return true;
}
CAT_TEST(FDreamReadableCopy,"ReadableCardsAndLegacyDescriptionRouting")
bool FDreamReadableCopy::RunTest(const FString&)
{
    for(const auto& Def:DreamAugmentCatalog::All())
    {
        TestFalse(*FString::Printf(TEXT("%s has concise card copy"),*Def.Code),Def.CardText.IsEmpty());
        TestTrue(*FString::Printf(TEXT("%s fits card copy budget"),*Def.Code),Def.CardText.Len()<=120);
        TestEqual(*FString::Printf(TEXT("%s uses same description across legacy and new entries"),*Def.Code),UDispatchTableComponent::GetAugmentDescription(Def.ID).ToString(),Def.CardText);
        if(Def.Evolution&&!Def.Permanent)
        {
            TestTrue(TEXT("Skill card shows stamina cost"),Def.CardText.Contains(TEXT("스태미나")));
            TestTrue(TEXT("Skill card shows cooldown"),Def.CardText.Contains(TEXT("재사용")));
        }
    }
    const auto* Health=DreamAugmentCatalog::Find(EAugmentID::HealthUp);
    TestTrue(TEXT("Health reward states actual recovery"),Health->CardText.Contains(TEXT("체력 20 회복")));
    const auto* Permanent=DreamAugmentCatalog::Find(EAugmentID::LastBreath);
    TestTrue(TEXT("Permanent card states health risk"),Permanent->CardText.Contains(TEXT("20%")));
    TestTrue(TEXT("Evolution card explains lost ingredient dependency"),Permanent->CardText.Contains(TEXT("재료 제거")));
    return true;
}

#endif
