#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "MainPlayerCharacter.h"
#include "CombatStatsComponent.h"
#include "BlackHoleZone.h"
#include "MonsterBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"

namespace DreamPatchTests
{
struct FWorld
{
    UWorld* World;
    FWorld()
    {
        UWorld::InitializationValues Values=UWorld::InitializationValues().AllowAudioPlayback(false)
            .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        World->InitializeActorsForPlay(FURL());World->BeginPlay();World->GetWorldSettings()->NotifyBeginPlay();
    }
    ~FWorld(){GEngine->DestroyWorldContext(World);World->DestroyWorld(false);}
    void Advance(float Seconds){for(float T=0;T<Seconds;T+=.01f){++GFrameCounter;World->Tick(LEVELTICK_All,.01f);}}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamGrowthTest,"DreamVeil.Patch.LevelGrowthRestoreDoesNotStack",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamGrowthTest::RunTest(const FString& Parameters)
{
    DreamPatchTests::FWorld Test;
    auto* Pawn=Test.World->SpawnActor<AMainPlayerCharacter>();
    if(!TestNotNull(TEXT("Player exists"),Pawn))return false;
    auto* Stats=Pawn->CombatStats.Get();Stats->InitStats(100,0,10);Stats->SetCurrentHealth(50);
    Pawn->RestoreLevelProgress(6,0);
    TestEqual(TEXT("Five level gains add 10 max health"),Stats->GetMaxHealth(),110.0f);
    TestEqual(TEXT("Five level gains add 1.25 attack"),Stats->GetAttackPower(),11.25f);
    TestEqual(TEXT("Growth does not heal"),Stats->GetCurrentHealth(),50.0f);
    Pawn->RestoreLevelProgress(6,0);
    TestEqual(TEXT("Repeated restoration does not stack health"),Stats->GetMaxHealth(),110.0f);
    TestEqual(TEXT("Repeated restoration does not stack attack"),Stats->GetAttackPower(),11.25f);
    Pawn->RestoreLevelProgress(1,0);
    TestEqual(TEXT("Reset removes only growth health"),Stats->GetMaxHealth(),100.0f);
    TestEqual(TEXT("Reset removes only growth attack"),Stats->GetAttackPower(),10.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamPullTest,"DreamVeil.Patch.OverlappingZonesCannotPinPlayer",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamPullTest::RunTest(const FString& Parameters)
{
    DreamPatchTests::FWorld Test;
    auto* Pawn=Test.World->SpawnActor<AMainPlayerCharacter>(FVector(200,0,10000),FRotator::ZeroRotator);
    auto* Caster=Test.World->SpawnActor<AMonsterBase>(FVector(3000,0,10000),FRotator::ZeroRotator);
    Pawn->GetCharacterMovement()->DisableMovement();Pawn->GetCharacterMovement()->MaxWalkSpeed=630;
    auto* First=Test.World->SpawnActorDeferred<ABlackHoleZone>(ABlackHoleZone::StaticClass(),FTransform(FVector(0,0,10000)),Caster,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    auto* Second=Test.World->SpawnActorDeferred<ABlackHoleZone>(ABlackHoleZone::StaticClass(),FTransform(FVector(0,0,10000)),Caster,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    for(auto* Zone:{First,Second})
    {
        FindFProperty<FFloatProperty>(Zone->GetClass(),TEXT("ArmDelay"))->SetPropertyValue_InContainer(Zone,0);
        FindFProperty<FFloatProperty>(Zone->GetClass(),TEXT("PullSpeed"))->SetPropertyValue_InContainer(Zone,500);
        Zone->FinishSpawning(FTransform(FVector(0,0,10000)));
    }
    const FVector Before=Pawn->GetActorLocation();Test.Advance(.1f);
    const float Distance=FVector::Dist2D(Before,Pawn->GetActorLocation());
    TestTrue(TEXT("Zones actually pull the player"),Distance>0.1f);
    TestTrue(TEXT("Two overlapping zones share 30% movement budget"),Distance<=630*.30f*.12f);
    First->Destroy();Second->Destroy();
    TestEqual(TEXT("Movement speed unchanged after zone removal"),Pawn->GetCharacterMovement()->MaxWalkSpeed,630.0f);
    return true;
}

#include "DispatchTableComponent.h"
#include "AugmentDamageLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamBerserkerTiers,"DreamVeil.Patch.ExistingAugments.BerserkerTiersPreserveOtherModifiers",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamBerserkerTiers::RunTest(const FString& Parameters)
{
    DreamPatchTests::FWorld Test;
    auto* Pawn=Test.World->SpawnActor<AMainPlayerCharacter>();
    auto* Stats=Pawn->CombatStats.Get();Stats->InitStats(100,0,10);
    Stats->MultiplyAttackPower(1.2f);
    Pawn->DispatchTable->ExecuteAugment(EAugmentID::Berserker);
    Stats->SetCurrentHealth(40);TestTrue(TEXT("40 percent activates 1.5 tier"),FMath::IsNearlyEqual(Stats->GetAttackPower(),18.0f));
    Stats->SetCurrentHealth(10);TestTrue(TEXT("10 percent replaces 1.5 with 2"),FMath::IsNearlyEqual(Stats->GetAttackPower(),24.0f));
    Stats->SetCurrentHealth(5);TestTrue(TEXT("Repeated critical updates do not stack"),FMath::IsNearlyEqual(Stats->GetAttackPower(),24.0f));
    Stats->SetCurrentHealth(11);TestTrue(TEXT("Healing returns to 1.5 tier"),FMath::IsNearlyEqual(Stats->GetAttackPower(),18.0f));
    Stats->AddAttackPower(3);TestTrue(TEXT("Additive growth is multiplied normally"),FMath::IsNearlyEqual(Stats->GetAttackPower(),23.4f,KINDA_SMALL_NUMBER));
    Stats->SetCurrentHealth(41);TestTrue(TEXT("Healing preserves independent modifier"),FMath::IsNearlyEqual(Stats->GetAttackPower(),15.6f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamVampireBudget,"DreamVeil.Patch.ExistingAugments.VampireActualDamageAndBudget",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamVampireBudget::RunTest(const FString& Parameters)
{
    DreamPatchTests::FWorld Test;
    auto* Pawn=Test.World->SpawnActor<AMainPlayerCharacter>();
    auto* Stats=Pawn->CombatStats.Get();Stats->InitStats(100,0,10);Stats->SetCurrentHealth(50);
    Pawn->DispatchTable->ExecuteAugment(EAugmentID::Vampire);
    for(int I=0;I<10;++I)Pawn->DispatchTable->ProcessOnDamageDealt(100);
    TestTrue(TEXT("Many hits share one percent maximum health per second"),FMath::IsNearlyEqual(Stats->GetCurrentHealth(),51.0f));
    Test.Advance(1.05f);Pawn->DispatchTable->ProcessOnDamageDealt(100);
    TestTrue(TEXT("Budget replenishes after one game-time second"),FMath::IsNearlyEqual(Stats->GetCurrentHealth(),51.35f));
    auto* OtherPawn=Test.World->SpawnActor<AMainPlayerCharacter>(FVector(0,0,10000),FRotator::ZeroRotator);
    auto* OtherStats=OtherPawn->CombatStats.Get();OtherStats->InitStats(100,0,10);OtherStats->SetCurrentHealth(50);
    OtherPawn->DispatchTable->ExecuteAugment(EAugmentID::Vampire);
    auto* Victim=Test.World->SpawnActor<AMonsterBase>(FVector(1000,0,10000),FRotator::ZeroRotator);
    Victim->MonsterCombatStats->InitStats(1,0,0);
    UAugmentDamageLibrary::ProcessIncomingDamage(Victim,1000,nullptr,nullptr,OtherPawn);
    TestTrue(TEXT("One remaining victim health cannot heal from 1000 overkill"),FMath::IsNearlyEqual(OtherStats->GetCurrentHealth(),50.0035f));
    return true;
}
#endif
