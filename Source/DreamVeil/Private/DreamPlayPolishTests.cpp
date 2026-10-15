#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "DreamPlayPolish.h"
#include "DreamAugmentCatalog.h"
#include "DreamVeilGameInstance.h"
#include "DreamAugmentFeedback.h"
#include "DreamObjectPoolSubsystem.h"
#include "DispatchTableComponent.h"
#include "MainPlayerCharacter.h"
#include "MainPlayerController.h"
#include "DreamRunSummaryWidget.h"
#include "Components/TextBlock.h"
#include "CombatStatsComponent.h"
#include "MonsterBase.h"
#include "MonsterSkill.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
namespace PlayPolishTests
{
struct FWorld
{
    UDreamVeilGameInstance* GI;UWorld* W;
    FWorld(){GI=NewObject<UDreamVeilGameInstance>(GEngine);GI->AddToRoot();const FString Package=FString(TEXT("/Temp/PlayPolish/"))+FGuid::NewGuid().ToString()+TEXT("/LV_2");GI->InitializeStandalone(FName(TEXT("LV_2")),CreatePackage(*Package));W=GI->GetWorld();W->InitializeActorsForPlay(FURL());W->BeginPlay();W->GetWorldSettings()->NotifyBeginPlay();Advance(.05f);}
    ~FWorld(){GI->Shutdown();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);GI->RemoveFromRoot();}
    void Advance(float S){for(float T=0;T<S;T+=.02f){++GFrameCounter;W->Tick(LEVELTICK_All,.02f);}}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamPressure,"DreamVeil.Polish.SpawnPressureNeverAcceleratesOriginalPace",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamPressure::RunTest(const FString&)
{
    TestEqual(TEXT("Empty map keeps original pace"),UDreamEncounterDirector::SpawnIntervalMultiplier(0,30,1,10,0),1.f);
    TestTrue(TEXT("Crowded map gets breathing room"),UDreamEncounterDirector::SpawnIntervalMultiplier(27,30,1,10,0)>1.5f);
    TestTrue(TEXT("Recent damage at low HP is gentler"),UDreamEncounterDirector::SpawnIntervalMultiplier(5,30,.2f,1,0)>1.4f);
    TestEqual(TEXT("Low HP alone cannot permanently stall a low-health build"),UDreamEncounterDirector::SpawnIntervalMultiplier(5,30,.2f,10,0),1.f);
    TestTrue(TEXT("Burst packs get a short recovery interval"),UDreamEncounterDirector::SpawnIntervalMultiplier(4,30,1,10,3)>1.3f);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamGoal,"DreamVeil.Polish.EvolutionGoalTracksMissingAndCompletedIngredients",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamGoal::RunTest(const FString&)
{
    PlayPolishTests::FWorld W;auto* Pref=W.GI->GetSubsystem<UDreamPlayPreferences>();auto* P=W.W->SpawnActor<AMainPlayerCharacter>();auto* D=P->DispatchTable.Get();
    TestFalse(TEXT("Base augment cannot be an evolution goal"),Pref->TrackEvolution(EAugmentID::AttackUp));TestTrue(TEXT("Evolution goal accepted"),Pref->TrackEvolution(EAugmentID::SkillExplosion));
    TestTrue(TEXT("No ingredients yet"),Pref->GetGoalText(D).Contains(TEXT("0/3")));
    for(auto ID:DreamAugmentCatalog::Find(EAugmentID::SkillExplosion)->Ingredients)D->ApplyAugment(ID);
    TestTrue(TEXT("All ingredients counted"),Pref->GetGoalText(D).Contains(TEXT("3/3")));TestTrue(TEXT("Goal never grants a free skill"),D->GetSkillCount()==0);
    Pref->ClearEvolutionGoal();TestTrue(TEXT("Goal cleared"),Pref->GetGoalText(D).Contains(TEXT("목표 없음")));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamAttackSpacing,"DreamVeil.Polish.AttackSpacingAndPoolReturnReleaseReservations",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamAttackSpacing::RunTest(const FString&)
{
    PlayPolishTests::FWorld W;auto* Flow=W.W->GetSubsystem<UDreamEncounterDirector>();auto* A=W.W->SpawnActor<AMonsterBase>(FVector(0,0,1000),FRotator::ZeroRotator);auto* B=W.W->SpawnActor<AMonsterBase>(FVector(500,0,1000),FRotator::ZeroRotator);auto* C=W.W->SpawnActor<AMonsterBase>(FVector(1000,0,1000),FRotator::ZeroRotator);
    TestTrue(TEXT("First heavy windup starts"),Flow->ReserveAttack(A,true));TestFalse(TEXT("Second simultaneous heavy is spaced"),Flow->ReserveAttack(B,true));W.Advance(.7f);TestTrue(TEXT("Second heavy allowed after cue window"),Flow->ReserveAttack(B,true));W.Advance(.7f);TestFalse(TEXT("At most two heavy skills overlap"),Flow->ReserveAttack(C,true));
    A->OnPoolReturned();TestTrue(TEXT("Returning attacker releases its slot"),Flow->ReserveAttack(C,true));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamReport,"DreamVeil.Polish.ReportFreezesOnceAndTelemetryCanStopSafely",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamReport::RunTest(const FString&)
{
    PlayPolishTests::FWorld W;auto* Flow=W.W->GetSubsystem<UDreamEncounterDirector>();auto* Pref=W.GI->GetSubsystem<UDreamPlayPreferences>();auto* P=W.W->SpawnActor<AMainPlayerCharacter>();auto* M=W.W->SpawnActor<AMonsterBase>(FVector(1000,0,1000),FRotator::ZeroRotator);
    Flow->RecordDamage(P,M,12,false,false);Flow->RecordDamage(P,M,7,true,false);Flow->RecordKill();Flow->RecordSkill();Flow->Finish(TEXT("검사"));
    TestEqual(TEXT("Actual health loss sum"),Pref->LastReport.DamageTaken,19.f);TestEqual(TEXT("Kill recorded once"),Pref->LastReport.Kills,1);TestEqual(TEXT("Skill activation count"),Pref->LastReport.SkillsUsed,1);
    Flow->RecordKill();Flow->Finish(TEXT("다른 결과"));TestEqual(TEXT("Final report cannot be overwritten"),Pref->LastReport.Outcome,FString(TEXT("검사")));TestEqual(TEXT("Late kills cannot alter frozen report"),Pref->LastReport.Kills,1);
    TestTrue(TEXT("Telemetry starts on demand"),Flow->StartPerformanceCapture(TEXT("PolishAutomation")));TestFalse(TEXT("Second capture cannot overwrite current one"),Flow->StartPerformanceCapture(TEXT("Other")));const FString File=Flow->StopPerformanceCapture();TestTrue(TEXT("CSV written"),IFileManager::Get().FileExists(*File));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamSimpleWarnings,"DreamVeil.Polish.SimplifiedEffectsKeepThreatWarningsAndEssentialZones",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamSimpleWarnings::RunTest(const FString&)
{
    PlayPolishTests::FWorld W;
    auto* Pref=W.GI->GetSubsystem<UDreamPlayPreferences>();
    const bool Original=Pref->IsSimplifiedEffects();
    Pref->SetSimplifiedEffects(true);
    auto* Source=W.W->SpawnActor<AActor>();
    auto* Feedback=W.W->GetSubsystem<UDreamAugmentFeedback>();
    for(int32 I=0;I<12;++I)
    {
        TestNotNull(TEXT("Optional effects fit the simplified budget"),UDreamAugmentFeedback::Emit(Source,EAugmentID::Knockback,EDreamAugmentCueShape::Ring,FVector(I*100,0,0),35,10));
        W.Advance(.35f);
    }
    TestEqual(TEXT("Simplified optional effect budget"),Feedback->GetActiveCueCount(),12);
    TestNull(TEXT("Further optional effects are suppressed"),UDreamAugmentFeedback::Emit(Source,EAugmentID::Knockback,EDreamAugmentCueShape::Ring,FVector::ZeroVector,35,10));
    auto* Monster=W.W->SpawnActor<AMonsterBase>(FVector(1000,0,1000),FRotator::ZeroRotator);
    auto* Warning=UDreamAugmentFeedback::Emit(Monster,EAugmentID::StopMark,EDreamAugmentCueShape::Mark,Monster->GetActorLocation(),65,1,Monster);
    TestNotNull(TEXT("Enemy warning can still start at the optional limit"),Warning);
    if(Warning){Warning->ThreatTint(FLinearColor::Red);TestTrue(TEXT("Warning cannot be evicted as optional decoration"),Warning->IsEssential());}
    auto* Field=UDreamAugmentFeedback::Emit(Source,EAugmentID::Knockback,EDreamAugmentCueShape::Field,FVector::ZeroVector,200,2);
    TestNotNull(TEXT("Gameplay zone remains visible at the optional limit"),Field);
    UDreamAugmentFeedback::StopSource(Monster);
    if(Warning)TestFalse(TEXT("Returning monster removes its warning"),Warning->IsPlaying());
    Pref->SetSimplifiedEffects(Original);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamExistingGameOver,"DreamVeil.Polish.ReportAttachesToExistingGameOverWithoutReplacingButtons",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamExistingGameOver::RunTest(const FString&)
{
    PlayPolishTests::FWorld W;
    auto* Flow=W.W->GetSubsystem<UDreamEncounterDirector>();
    Flow->RecordKill();Flow->Finish(TEXT("검사"));
    auto* Class=LoadClass<UUserWidget>(nullptr,TEXT("/Game/UI/WBP_GameOver.WBP_GameOver_C"));
    TestNotNull(TEXT("Original game-over blueprint is available"),Class);
    auto* Host=Class?CreateWidget<UUserWidget>(W.GI,Class):nullptr;
    TestNotNull(TEXT("Original game-over screen initializes"),Host);
    if(!Host)return false;
    auto* OriginalRoot=Host->GetRootWidget();
    UDreamRunSummaryWidget::AttachSummary(Host);
    auto* Summary=Cast<UTextBlock>(Host->GetWidgetFromName(TEXT("EncounterSummaryText")));
    TestNotNull(TEXT("Report is attached to actual game-over asset"),Summary);
    UDreamRunSummaryWidget::AttachSummary(Host);
    TestEqual(TEXT("Root layout and existing buttons are retained"),Host->GetRootWidget(),OriginalRoot);
    TestEqual(TEXT("Repeated presentation reuses report text"),Host->GetWidgetFromName(TEXT("EncounterSummaryText")),static_cast<UWidget*>(Summary));
    return true;
}
#endif
