#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "DreamVeilGameInstance.h"
#include "BossMonster.h"
#include "BlackHoleZone.h"
#include "MainGameModeBase.h"
#include "DreamBalanceProfile.h"
#include "DreamAugmentRuntime.h"
#include "MonsterSkill.h"
#include "EliteMonster.h"
#include "MonsterAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "DreamBossHealthWidget.h"
#include "Widgets/SWidget.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "MonsterProgressionLibrary.h"
#include "MonsterBase.h"
#include "MainPlayerCharacter.h"
#include "DispatchTableComponent.h"
#include "CombatStatsComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundAttenuation.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UnrealType.h"

namespace DreamFeedbackTests
{
struct FWorld
{
    UDreamVeilGameInstance* GI;
    UWorld* World;
    explicit FWorld(const TCHAR* Map,bool CombatMode=false)
    {
        GI=NewObject<UDreamVeilGameInstance>(GEngine);GI->AddToRoot();
        const FString Package=FString(TEXT("/Temp/DreamFeedback/"))+FGuid::NewGuid().ToString()+TEXT("/")+Map;
        GI->InitializeStandalone(FName(Map),CreatePackage(*Package));World=GI->GetWorld();
        if(CombatMode){World->GetWorldSettings()->DefaultGameMode=AMainGameModeBase::StaticClass();World->SetGameMode(FURL());}
        World->InitializeActorsForPlay(FURL());World->BeginPlay();World->GetWorldSettings()->NotifyBeginPlay();
    }
    ~FWorld(){GI->Shutdown();GEngine->DestroyWorldContext(World);World->DestroyWorld(false);GI->RemoveFromRoot();}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamFinalBossScope,"DreamVeil.Feedback.AllBossNerfPreservesStageScalingAndPoolReuse",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamFinalBossScope::RunTest(const FString&)
{
    auto* BossClass=LoadClass<ABossMonster>(nullptr,TEXT("/Game/Blueprint/Monsters/BP_BossMonster.BP_BossMonster_C"));
    if(!TestNotNull(TEXT("Actual boss blueprint"),BossClass))return false;
    for(const TCHAR* Map:{TEXT("LV_1"),TEXT("LV_2"),TEXT("LV_3"),TEXT("LV_4"),TEXT("Endless")})
    {
        DreamFeedbackTests::FWorld W(Map);
        auto* Boss=W.World->SpawnActor<ABossMonster>(BossClass,FVector(0,0,10000),FRotator::ZeroRotator);
        if(!TestNotNull(TEXT("Boss spawned"),Boss))return false;
        const auto Read=[&](const TCHAR* Name){return FindFProperty<FFloatProperty>(Boss->GetClass(),Name)->GetPropertyValue_InContainer(Boss);};
        const float BaseHealth=Read(TEXT("MinHealthRadius")),BaseDamage=Read(TEXT("MonsterDamage"));
        const bool Final=W.GI->GetCurrentLevelNumber()==4;
        TestEqual(TEXT("Expected real map classification"),Final,FString(Map)==TEXT("LV_4"));
        const float Health=BaseHealth*UMonsterProgressionLibrary::GetMonsterHealthScale(Boss,EMonsterGrade::Boss)*(Final?.85f:1.f)*.8f,Damage=BaseDamage*UMonsterProgressionLibrary::GetMonsterDamageScale(Boss,EMonsterGrade::Boss)*(Final?.9f:1.f)*.85f;
        TestTrue(TEXT("All boss health reduced including Endless"),FMath::IsNearlyEqual(Boss->MonsterCombatStats->GetMaxHealth(),Health,.1f));
        TestTrue(TEXT("All boss attacks reduced including Endless"),FMath::IsNearlyEqual(Boss->MonsterCombatStats->GetAttackPower(),Damage,.01f));
        const float SkillScale=Boss->GetSkillDamageScale();Boss->OnPoolAcquired();
        TestTrue(TEXT("Pool reacquisition does not compound the nerf"),FMath::IsNearlyEqual(Boss->MonsterCombatStats->GetMaxHealth(),Health,.1f));
        TestTrue(TEXT("Skill damage follows same multiplier after reuse"),FMath::IsNearlyEqual(Boss->GetSkillDamageScale(),SkillScale,.01f));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamMonsterAudioPool,"DreamVeil.Feedback.MonsterAudioSharesBoundedVoicesAndResetsDeathOnReuse",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamMonsterAudioPool::RunTest(const FString&)
{
    DreamFeedbackTests::FWorld W(TEXT("LV_1"));
    auto* First=W.World->SpawnActor<AMonsterBase>(FVector(0,0,10000),FRotator::ZeroRotator);
    auto* Second=W.World->SpawnActor<AMonsterBase>(FVector(500,0,10000),FRotator::ZeroRotator);
    for(auto* Monster:{First,Second})
    {
        TestNotNull(TEXT("Attack sound"),Monster->AttackWhooshSound.Get());TestNotNull(TEXT("Charge sound"),Monster->ChargeWhooshSound.Get());TestNotNull(TEXT("Death sound"),Monster->DeathSound.Get());
        TestTrue(TEXT("Positional attenuation"),Monster->CombatSoundAttenuation->Attenuation.bSpatialize&&Monster->CombatSoundAttenuation->Attenuation.bAttenuate);
        Monster->MonsterCombatStats->SetCurrentHealth(0);TestTrue(TEXT("Death cue committed once for this life"),Monster->bDeathSoundPlayed);
        Monster->PrepareForPoolReuse();Monster->OnPoolAcquired();TestFalse(TEXT("Reused monster may play its next death cue"),Monster->bDeathSoundPlayed);
    }
    TestTrue(TEXT("All monsters share one sound concurrency asset"),First->CombatSoundConcurrency==Second->CombatSoundConcurrency);
    TestEqual(TEXT("Bounded group size"),First->CombatSoundConcurrency->Concurrency.MaxCount,12);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamBossBarScope,"DreamVeil.Feedback.CampaignBossBarTracksHealthAndExcludesEndless",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamBossBarScope::RunTest(const FString&)
{
    for(const TCHAR* Map:{TEXT("LV_1"),TEXT("LV_2"),TEXT("LV_3"),TEXT("LV_4"),TEXT("Endless")})
    {
        DreamFeedbackTests::FWorld W(Map);
        auto* B=W.World->SpawnActor<ABossMonster>(FVector(0,0,10000),FRotator::ZeroRotator);
        // Native widget initialization requires a local-player context, as in an actual viewport.
        auto* Local=NewObject<ULocalPlayer>(GEngine);
        auto* Controller=W.World->SpawnActor<APlayerController>();Local->PlayerController=Controller;Controller->Player=Local;
        auto* Bar=NewObject<UDreamBossHealthWidget>(Local);
        Bar->SetPlayerContext(FLocalPlayerContext(Local,W.World));Bar->Initialize();
        if(!TestNotNull(TEXT("Native boss HUD initializes"),Bar))return false;
        auto Slate=Bar->TakeWidget();
        TestNull(TEXT("Boss HUD has no health caption"),Bar->WidgetTree->FindWidget(TEXT("BossHealthCaption")));
        auto* Notice=Cast<UTextBlock>(Bar->WidgetTree->FindWidget(TEXT("BossDefeatedAnnouncement")));
        TestNotNull(TEXT("Center defeat notice exists"),Notice);
        if(Notice)TestEqual(TEXT("Defeat notice uses requested wording"),Notice->GetText().ToString(),FString(TEXT("보스 처치")));
        if(W.GI->IsInEndless())
        {TestNull(TEXT("No boss bar in Endless"),Bar->GetDisplayedBoss());continue;}
        TestEqual(TEXT("Campaign HUD discovers live boss"),Bar->GetDisplayedBoss(),B);
        B->MonsterCombatStats->SetCurrentHealth(B->MonsterCombatStats->GetMaxHealth()*.37f);
        TestTrue(TEXT("HUD reads actual changing health"),FMath::IsNearlyEqual(Bar->GetDisplayedHealthPercent(),.37f,.001f));
        B->SetActorHiddenInGame(true);TestNull(TEXT("Hidden/returned bosses cannot retain the HUD"),Bar->GetDisplayedBoss());
        B->SetActorHiddenInGame(false);B->MonsterCombatStats->SetCurrentHealth(0);
        TestNull(TEXT("Dead boss disappears immediately"),Bar->GetDisplayedBoss());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamStageHealthRestore,"DreamVeil.Feedback.StageEntryFillsFinalGrowthAndAugmentHealth",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamStageHealthRestore::RunTest(const FString&)
{
    DreamFeedbackTests::FWorld W(TEXT("LV_1"));
    auto* LocalPlayer=NewObject<ULocalPlayer>(GEngine);
    W.GI->AddLocalPlayer(LocalPlayer,FPlatformUserId::CreateFromInternalId(0));
    if(!TestNotNull(TEXT("Local player used by progress snapshot"),LocalPlayer))return false;
    auto* PC=W.World->SpawnActor<APlayerController>();PC->SetPlayer(LocalPlayer);
    auto* Source=W.World->SpawnActor<AMainPlayerCharacter>();PC->Possess(Source);
    Source->RestoreLevelProgress(11,0);
    Source->DispatchTable->ApplyAugment(EAugmentID::HealthUp);
    Source->CombatStats->SetCurrentHealth(90);
    // Request the normal travel path to capture the real private level snapshot.
    // Do not tick this synthetic world; the queued map travel is discarded at teardown.
    TestTrue(TEXT("Snapshot through stage entry"),W.GI->OpenLevelByNumber(1));
    auto* Restored=W.World->SpawnActor<AMainPlayerCharacter>(FVector(1000,0,0),FRotator::ZeroRotator);
    if(!TestNotNull(TEXT("Fresh stage pawn"),Restored))return false;
    TestEqual(TEXT("Level restored before final refill"),Restored->GetPlayerLevel(),11);
    TestEqual(TEXT("Augment and level maximum restored"),Restored->CombatStats->GetMaxHealth(),140.f);
    TestEqual(TEXT("Final maximum filled instead of base 100"),Restored->CombatStats->GetCurrentHealth(),140.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamDifficultyAllModes,"DreamVeil.Feedback.DifficultyRatiosAndCommonNerfAllMapsAndGrades",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamDifficultyAllModes::RunTest(const FString&)
{
    const EGameDifficulty Modes[]={EGameDifficulty::SuperEasy,EGameDifficulty::Easy,EGameDifficulty::Normal,EGameDifficulty::Hard};
    const float Ratios[]={.25f,.5f,1.f,2.5f};
    for(const TCHAR* Map:{TEXT("LV_1"),TEXT("LV_2"),TEXT("LV_3"),TEXT("LV_4"),TEXT("Endless")})
    {
        DreamFeedbackTests::FWorld W(Map);
        auto* Profile=W.GI->GetBalanceProfile();if(!TestNotNull(TEXT("Actual balance data asset"),Profile))return false;
        for(int32 I=0;I<4;++I)
        {
            W.GI->SetDifficulty(Modes[I]);
            TestEqual(TEXT("User difficulty ratio"),UMonsterProgressionLibrary::GetDifficultyStatScale(W.World),Ratios[I]);
            for(auto Grade:{EMonsterGrade::Normal,EMonsterGrade::Elite,EMonsterGrade::Boss})
            {
                const float Growth=UMonsterProgressionLibrary::GetStageStatScale(W.World)*UMonsterProgressionLibrary::GetGradeStatScale(Grade);
                TestTrue(TEXT("Health ratio and 30 percent reduction"),FMath::IsNearlyEqual(UMonsterProgressionLibrary::GetMonsterHealthScale(W.World,Grade),Ratios[I]*Growth*.7f,.001f));
                TestTrue(TEXT("Damage ratio and 30 percent reduction"),FMath::IsNearlyEqual(UMonsterProgressionLibrary::GetMonsterDamageScale(W.World,Grade),Ratios[I]*FMath::Sqrt(Growth)*.7f));
                const float XP=UMonsterProgressionLibrary::GetExperienceReward(W.World,Grade);
                W.GI->SetDifficulty(EGameDifficulty::Normal);
                TestEqual(TEXT("Difficulty nerf preserves XP rewards"),UMonsterProgressionLibrary::GetExperienceReward(W.World,Grade),XP);
                W.GI->SetDifficulty(Modes[I]);
            }
            TestEqual(TEXT("All modes get common reduced spawn rate"),UMonsterProgressionLibrary::GetSpawnRateMultiplier(W.World),.7f);
        }
    }
    auto* Class=LoadClass<AMainGameModeBase>(nullptr,TEXT("/Game/Blueprint/BP_MainGameModeBase.BP_MainGameModeBase_C"));
    TestNotNull(TEXT("Real game mode BP"),Class);if(Class)TestEqual(TEXT("Serialized Endless cadence is 60 seconds"),Class->GetDefaultObject<AMainGameModeBase>()->GetEndlessBossInterval(),60.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamBossClearScope,"DreamVeil.Feedback.BossDeathKillsCampaignSurvivorsAndStopsQueuedSpawns",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamBossClearScope::RunTest(const FString&)
{
    for(const TCHAR* Map:{TEXT("LV_1"),TEXT("LV_2"),TEXT("LV_3"),TEXT("LV_4"),TEXT("Endless"),TEXT("Lobby")})
    {
        DreamFeedbackTests::FWorld W(Map,true);auto* Mode=W.World->GetAuthGameMode<AMainGameModeBase>();
        if(!TestNotNull(TEXT("Actual campaign game mode"),Mode))return false;
        auto* B=W.World->SpawnActor<ABossMonster>(FVector(0,0,10000),FRotator::ZeroRotator);
        auto* M=W.World->SpawnActor<AMonsterBase>(FVector(500,0,10000),FRotator::ZeroRotator);
        auto* Elite=W.World->SpawnActor<AEliteMonster>(FVector(1000,0,10000),FRotator::ZeroRotator);
        FActorSpawnParameters HazardParams;HazardParams.Owner=B;HazardParams.Instigator=B;
        auto* Hazard=W.World->SpawnActor<ABlackHoleZone>(ABlackHoleZone::StaticClass(),FVector(1500,0,10000),FRotator::ZeroRotator,HazardParams);
        B->MonsterCombatStats->SetCurrentHealth(0);
        const bool Campaign=W.GI->IsInLevelMap()&&!W.GI->IsInEndless();
        TestEqual(TEXT("Immediate boss defeat presentation only LV1-4"),Mode->IsBossDefeatPresentation(),Campaign);
        TestEqual(TEXT("Normal survivor killed only LV1-4"),M->MonsterCombatStats->IsDead(),Campaign);
        TestEqual(TEXT("Elite survivor killed only LV1-4"),Elite->MonsterCombatStats->IsDead(),Campaign);
        TestEqual(TEXT("Enemy lingering hazard removed only LV1-4"),!IsValid(Hazard)||Hazard->IsActorBeingDestroyed()||Hazard->IsHidden(),Campaign);
        if(Campaign){TestFalse(TEXT("Queued groups cannot spawn during defeat notice"),Mode->CanSpawnRegularMonster());Mode->NotifyBossDefeated(B);TestTrue(TEXT("Repeated boss death is harmless"),Mode->IsBossDefeatPresentation());}
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamDecoyPriority,"DreamVeil.Feedback.DecoyPreemptsPlayerAndRestoresTargetsOnExpiry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamDecoyPriority::RunTest(const FString&)
{
    DreamFeedbackTests::FWorld W(TEXT("LV_1"));
    TestNotNull(TEXT("Fixture AI system"),W.World->CreateAISystem());
    auto* LP=NewObject<ULocalPlayer>(GEngine);W.GI->AddLocalPlayer(LP,FPlatformUserId::CreateFromInternalId(0));
    auto* PC=W.World->SpawnActor<APlayerController>();PC->SetPlayer(LP);
    auto* P=W.World->SpawnActor<AMainPlayerCharacter>(FVector(0,0,10000),FRotator::ZeroRotator);PC->Possess(P);
    TArray<AMonsterBase*> Enemies={W.World->SpawnActor<AMonsterBase>(FVector(150,0,10000),FRotator::ZeroRotator),W.World->SpawnActor<AEliteMonster>(FVector(200,100,10000),FRotator::ZeroRotator),W.World->SpawnActor<ABossMonster>(FVector(250,-100,10000),FRotator::ZeroRotator)};
    for(auto* M:Enemies)
    {
        auto* AI=W.World->SpawnActor<AMonsterAIController>();AI->Possess(M);
        UBlackboardComponent* Initialized=nullptr;
        auto* Data=LoadObject<UBlackboardData>(nullptr,TEXT("/Game/Managers/AI/BB_Monster.BB_Monster"));
        if(!TestTrue(TEXT("Explicitly initialize fixture AI blackboard"),Data&&AI->UseBlackboard(Data,Initialized)))return false;
        if(auto* BB=AI->GetBlackboardComponent())BB->SetValueAsObject(TEXT("PlayerActor"),P);
        AddInfo(FString::Printf(TEXT("Decoy fixture %s alive=%d hidden=%d player=%s"),*M->GetName(),!M->MonsterCombatStats->IsDead(),M->IsHidden(),*GetNameSafe(UGameplayStatics::GetPlayerPawn(W.World,0))));
        if(auto* BB=AI->GetBlackboardComponent())
        {
            const auto Key=BB->GetKeyID(TEXT("PlayerActor"));const auto* Entry=BB->GetBlackboardAsset()?BB->GetBlackboardAsset()->GetKey(Key):nullptr;const auto* Type=Entry?Cast<UBlackboardKeyType_Object>(Entry->KeyType.Get()):nullptr;
            AddInfo(FString::Printf(TEXT("BB key %d type=%s value=%s"),int32(Key),Type?*GetNameSafe(Type->BaseClass):TEXT("non-instanced"),*GetNameSafe(BB->GetValueAsObject(TEXT("PlayerActor")))));
        }
        M->StartAttack(P);
    }
    auto* D=W.World->SpawnActor<ADreamAugmentField>(FVector(50,100,10000),FRotator::ZeroRotator);D->Configure(P,2,300,3,0);
    for(auto* M:Enemies)
    {
        TestEqual(TEXT("Every nearby enemy grade prioritizes visible decoy"),ADreamAugmentField::FindDecoyFor(M,P),static_cast<APawn*>(D));
        auto* AI=Cast<AMonsterAIController>(M->GetController());auto* BB=AI?AI->GetBlackboardComponent():nullptr;
        AddInfo(FString::Printf(TEXT("After decoy %s hidden=%d health=%f target=%s"),*M->GetName(),M->IsHidden(),M->MonsterCombatStats->GetCurrentHealth(),BB?*GetNameSafe(BB->GetValueAsObject(TEXT("PlayerActor"))):TEXT("null")));
        if(TestNotNull(TEXT("Pursuit blackboard"),BB))TestEqual(TEXT("Existing pursuit immediately retargets"),BB->GetValueAsObject(TEXT("PlayerActor")),static_cast<UObject*>(D));
        TestFalse(TEXT("Old player attack cancelled on decoy creation"),M->IsAttacking());
    }
    D->Expire();
    for(auto* M:Enemies)
    {
        TestEqual(TEXT("Expiry returns to player"),ADreamAugmentField::FindDecoyFor(M,P),static_cast<APawn*>(P));
        auto* AI=Cast<AMonsterAIController>(M->GetController());auto* BB=AI?AI->GetBlackboardComponent():nullptr;
        if(BB)TestEqual(TEXT("Expiry immediately refreshes pursuit"),BB->GetValueAsObject(TEXT("PlayerActor")),static_cast<UObject*>(P));
    }
    return true;
}

#endif
