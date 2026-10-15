#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DreamAugmentFeedback.h"
#include "DreamAugmentCatalog.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DreamAugmentRuntime.h"
#include "DreamObjectPoolSubsystem.h"
#include "DispatchTableComponent.h"
#include "MainPlayerCharacter.h"
#include "MainPlayerController.h"
#include "CombatStatsComponent.h"
#include "MonsterBase.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
namespace AugmentFeedbackTests
{
struct FWorld
{
    UWorld* World;
    FWorld(){auto V=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&V);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());World->BeginPlay();World->GetWorldSettings()->NotifyBeginPlay();}
    ~FWorld(){GEngine->DestroyWorldContext(World);World->DestroyWorld(false);}
    void Advance(float Seconds){for(float T=0;T<Seconds;T+=.02f){++GFrameCounter;World->Tick(LEVELTICK_All,.02f);}}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamAugmentCueBudget,"DreamVeil.AugmentFeedback.BoundedBurstPoolAndNoCollision",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamAugmentCueBudget::RunTest(const FString&)
{
    AugmentFeedbackTests::FWorld W;auto* Source=W.World->SpawnActor<AActor>();auto* S=W.World->GetSubsystem<UDreamAugmentFeedback>();
    auto* First=UDreamAugmentFeedback::Emit(Source,EAugmentID::SkillExplosion,EDreamAugmentCueShape::Ring,FVector(0,0,10),300,.2f);
    if(!TestNotNull(TEXT("Ring spawned"),First))return false;
    TestFalse(TEXT("Visual actor cannot block movement or shots"),First->GetActorEnableCollision());
    TArray<UStaticMeshComponent*> Meshes;First->GetComponents(Meshes);
    for(auto* Mesh:Meshes){TestFalse(TEXT("No visual mesh collision"),Mesh->IsCollisionEnabled());TestNotNull(TEXT("Cookable material reference"),Mesh->GetMaterial(0));}
    for(int32 I=0;I<1000;++I)UDreamAugmentFeedback::Emit(Source,EAugmentID::SkillExplosion,EDreamAugmentCueShape::Impact,FVector(I,0,100),80,.2f);
    TestTrue(TEXT("At most ten priority emissions per 100 ms"),S->GetActiveCueCount()<=10);
    auto* Field=UDreamAugmentFeedback::Emit(Source,EAugmentID::SkillSuppress,EDreamAugmentCueShape::Field,FVector(0,0,10),250,.2f);
    TestNotNull(TEXT("Essential ground indicator survives a burst emission limit"),Field);
    W.Advance(.35f);TestEqual(TEXT("All burst effects expire"),S->GetActiveCueCount(),0);
    const int32 Before=W.World->GetSubsystem<UDreamObjectPoolSubsystem>()->GetActorReuseCount();
    auto* Reused=UDreamAugmentFeedback::Emit(Source,EAugmentID::SkillShield,EDreamAugmentCueShape::Aura,FVector(0,0,10),70,.2f,Source);
    TestNotNull(TEXT("Pool reacquires effect"),Reused);TestTrue(TEXT("Pool actually reuses geometry"),W.World->GetSubsystem<UDreamObjectPoolSubsystem>()->GetActorReuseCount()>Before);
    W.Advance(.3f);TestEqual(TEXT("Reused effect expires too"),S->GetActiveCueCount(),0);
    for(int32 Batch=0;Batch<4;++Batch){for(int32 I=0;I<8;++I)UDreamAugmentFeedback::Emit(Source,EAugmentID::SkillExplosion,EDreamAugmentCueShape::Impact,FVector(I*50,Batch*50,100),50,5);W.Advance(.11f);}
    TestEqual(TEXT("Hard active cap reached"),S->GetActiveCueCount(),32);
    auto* Essential=UDreamAugmentFeedback::Emit(Source,EAugmentID::SkillTrap,EDreamAugmentCueShape::Field,FVector(0,0,10),200,5);
    TestNotNull(TEXT("Essential field replaces a cosmetic burst at full capacity"),Essential);
    TestEqual(TEXT("Eviction and immediate reuse never duplicate a tracked actor"),S->GetActiveCueCount(),32);
    UDreamAugmentFeedback::StopSource(Source);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamAugmentCueLifetime,"DreamVeil.AugmentFeedback.TargetGenerationRefreshAndSourceReset",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamAugmentCueLifetime::RunTest(const FString&)
{
    AugmentFeedbackTests::FWorld W;auto* Source=W.World->SpawnActor<AActor>();
    auto* Target=Cast<AMonsterBase>(UDreamObjectPoolSubsystem::Spawn(W.World,AMonsterBase::StaticClass(),FTransform(FVector(1000,0,100))));
    auto* Cue=UDreamAugmentFeedback::Emit(Source,EAugmentID::ChillingRounds,EDreamAugmentCueShape::Mark,Target->GetActorLocation(),30,1,Target);
    auto* Refresh=UDreamAugmentFeedback::Emit(Source,EAugmentID::ChillingRounds,EDreamAugmentCueShape::Mark,Target->GetActorLocation(),30,2,Target);
    TestTrue(TEXT("Repeated hits refresh one marker"),Cue&&Cue==Refresh);W.Advance(1.1f);TestTrue(TEXT("Refresh extends life"),Cue->IsPlaying());
    Cast<IDreamPoolable>(Target)->AdvancePoolGeneration();W.Advance(.04f);
    TestFalse(TEXT("New target generation cannot inherit old-life marker"),Cue->IsPlaying());
    Cue=UDreamAugmentFeedback::Emit(Source,EAugmentID::ChillingRounds,EDreamAugmentCueShape::Mark,Target->GetActorLocation(),30,2,Target);
    UDreamObjectPoolSubsystem::ReleaseOrDestroy(Target);W.Advance(.04f);TestFalse(TEXT("Returning target immediately clears its marker"),Cue->IsPlaying());
    auto* Aura=UDreamAugmentFeedback::Emit(Source,EAugmentID::AutoShield,EDreamAugmentCueShape::Aura,Source->GetActorLocation(),70,10,Source);
    TestNotNull(TEXT("Aura exists"),Aura);UDreamAugmentFeedback::StopSource(Source);TestEqual(TEXT("Source reset clears all effects"),W.World->GetSubsystem<UDreamAugmentFeedback>()->GetActiveCueCount(),0);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamAugmentSkillPresentation,"DreamVeil.AugmentFeedback.PreparedSkillStateAndCosmeticsDoNotChangeCombat",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamAugmentSkillPresentation::RunTest(const FString&)
{
    AugmentFeedbackTests::FWorld W;auto* P=W.World->SpawnActor<AMainPlayerCharacter>();auto* PC=W.World->SpawnActor<AMainPlayerController>();PC->Possess(P);auto* D=P->DispatchTable.Get();
    TestTrue(TEXT("Restore existing evolution history"),D->RestoreAugments({EAugmentID::FocusFire,EAugmentID::AttackUp,EAugmentID::StaminaUp,EAugmentID::SkillOvercharge}));
    auto* R=D->GetRuntime();auto* S=W.World->GetSubsystem<UDreamAugmentFeedback>();TestTrue(TEXT("Save restore never shows an acquisition toast"),S->Notice(P).IsEmpty());
    W.Advance(20);const float Health=P->CombatStats->GetCurrentHealth(),Attack=P->CombatStats->GetAttackPower(),Stamina=P->GetCurrentStamina();
    TestTrue(TEXT("Prepare next shot"),D->TryUseSkill(0));TestTrue(TEXT("Prepared state visible before shot"),R->HasFeedbackState(EAugmentID::SkillOvercharge));
    TestEqual(TEXT("Preparing still costs no stamina"),P->GetCurrentStamina(),Stamina);
    TestTrue(TEXT("Cancel same prepared skill"),D->TryUseSkill(0));TestFalse(TEXT("Cancel clears presentation state"),R->HasFeedbackState(EAugmentID::SkillOvercharge));
    W.Advance(.1f);TestEqual(TEXT("No health changes from cosmetic cues"),P->CombatStats->GetCurrentHealth(),Health);TestEqual(TEXT("No attack changes from cosmetic cues"),P->CombatStats->GetAttackPower(),Attack);
    D->bAugmentFeedbackEnabled=false;R->Reset();
    TestNull(TEXT("Player may disable world effects"),UDreamAugmentFeedback::Emit(P,EAugmentID::SkillShield,EDreamAugmentCueShape::Ring,P->GetActorLocation()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamAllSkillFeedback,"DreamVeil.AugmentFeedback.AllThirtyEvolvedSkillsCastWithVisualFeedback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamAllSkillFeedback::RunTest(const FString&)
{
    AugmentFeedbackTests::FWorld W;
    auto* Floor=W.World->SpawnActor<AStaticMeshActor>(FVector(0,0,-50),FRotator::ZeroRotator);
    Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));Floor->SetActorScale3D(FVector(200,200,1));
    int32 Count=0;
    for(const auto& Def:DreamAugmentCatalog::All())
    {
        if(!Def.Evolution||Def.Permanent)continue;
        auto* P=W.World->SpawnActor<AMainPlayerCharacter>(FVector(0,0,100),FRotator::ZeroRotator);
        auto* PC=W.World->SpawnActor<AMainPlayerController>();PC->Possess(P);P->GetCharacterMovement()->DisableMovement();
        auto* D=P->DispatchTable.Get();auto History=Def.Ingredients;History.Add(Def.ID);
        TestTrue(*FString::Printf(TEXT("Recipe restores: %s"),*Def.Name),D->RestoreAugments(History));
        W.Advance(Def.Cooldown+1);
        P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        const float Before=P->GetCurrentStamina();
        TestTrue(*FString::Printf(TEXT("Actual cast succeeds: %s"),*Def.Name),D->TryUseSkill(0));
        TestTrue(*FString::Printf(TEXT("Visible cue produced: %s"),*Def.Name),W.World->GetSubsystem<UDreamAugmentFeedback>()->GetActiveCueCount()>0);
        const float Expected=Def.ID==EAugmentID::SkillOvercharge?Before:Before-D->GetSkillCost(0);
        TestTrue(TEXT("One original stamina charge only"),FMath::IsNearlyEqual(P->GetCurrentStamina(),Expected));
        D->GetRuntime()->Reset();TestEqual(TEXT("Skill reset clears own visual actors"),W.World->GetSubsystem<UDreamAugmentFeedback>()->GetActiveCueCount(),0);
        P->Destroy();PC->Destroy();W.Advance(.15f);++Count;
    }
    TestEqual(TEXT("Every evolved button skill covered"),Count,30);return true;
}

#endif
