#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "DreamVeilGameInstance.h"
#include "BossMonster.h"
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
    explicit FWorld(const TCHAR* Map)
    {
        GI=NewObject<UDreamVeilGameInstance>(GEngine);GI->AddToRoot();
        const FString Package=FString(TEXT("/Temp/DreamFeedback/"))+FGuid::NewGuid().ToString()+TEXT("/")+Map;
        GI->InitializeStandalone(FName(Map),CreatePackage(*Package));World=GI->GetWorld();
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
        const float Scale=UMonsterProgressionLibrary::GetMonsterStatScale(Boss,UMonsterProgressionLibrary::GetMonsterGrade(Boss));
        const bool Final=W.GI->GetCurrentLevelNumber()==4;
        TestEqual(TEXT("Expected real map classification"),Final,FString(Map)==TEXT("LV_4"));
        const float Health=BaseHealth*Scale*(Final?.85f:1.f)*.8f,Damage=BaseDamage*FMath::Sqrt(Scale)*(Final?.9f:1.f)*.85f;
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

#endif

