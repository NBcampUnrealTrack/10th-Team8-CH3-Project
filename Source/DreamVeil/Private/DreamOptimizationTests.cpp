#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "MainPlayerCharacter.h"
#include "MainPlayerController.h"
#include "DispatchTableComponent.h"
#include "DreamAugmentCatalog.h"
#include "CombatStatsComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UnrealType.h"
#include "HAL/PlatformTime.h"
#include "AugmentDamageLibrary.h"
#include "MonsterCollision.h"
#include "Components/SphereComponent.h"
#include "Kismet/KismetSystemLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamCachedAugmentsTest,"DreamVeil.Optimization.AugmentQueriesAfterStackEvolutionRestoreAndPoolReset",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamCachedAugmentsTest::RunTest(const FString&)
{
    auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());World->BeginPlay();World->GetWorldSettings()->NotifyBeginPlay();
    auto* Player=World->SpawnActor<AMainPlayerCharacter>();auto* Controller=World->SpawnActor<AMainPlayerController>();Controller->Possess(Player);
    auto* Table=Player->DispatchTable.Get();
    for(int32 I=0;I<300;++I)TestTrue(TEXT("Stacking still accepted"),Table->ApplyAugment(EAugmentID::AttackUp));
    for(auto ID:{EAugmentID::Knockback,EAugmentID::DefenceUp,EAugmentID::EffectRange,EAugmentID::AreaAttack,EAugmentID::Berserker,EAugmentID::LastFortress})TestTrue(TEXT("Recipe ingredients accepted"),Table->ApplyAugment(ID));
    for(auto ID:{EAugmentID::SkillExplosion,EAugmentID::SuppressionPulse,EAugmentID::LastBreath})
    {
        *FindFProperty<FArrayProperty>(Player->GetClass(),TEXT("CurrentAugmentChoices"))->ContainerPtrToValuePtr<TArray<EAugmentID>>(Player)={ID};
        TestTrue(TEXT("Evolutions commit their queries atomically"),Player->SelectAugmentChoice(ID));
    }
    const auto History=Table->GetAugmentHistory();
    auto Verify=[&](UDispatchTableComponent* Query)
    {
        TSet<EAugmentID> Passives,Actives;TArray<EAugmentID> Skills;
        for(auto ID:History)
        {
            const auto* Definition=DreamAugmentCatalog::Find(ID);
            if(Definition&&Definition->Evolution){if(!Definition->Permanent)Skills.AddUnique(ID);}
            else (UDispatchTableComponent::GetAugmentCategory(ID)==EAugmentCategory::Passive?Passives:Actives).Add(ID);
        }
        for(const auto& Definition:DreamAugmentCatalog::All())
        {
            int32 Expected=0;for(auto ID:History)Expected+=ID==Definition.ID;
            TestEqual(TEXT("Cached stacks match complete history"),Query->GetStackCount(Definition.ID),Expected);
            TestEqual(TEXT("Cached ownership matches history"),Query->HasAcquiredAugment(Definition.ID),Expected>0);
        }
        TestEqual(TEXT("Passive species"),Query->GetOwnedCount(EAugmentCategory::Passive),Passives.Num());
        TestEqual(TEXT("Active species"),Query->GetOwnedCount(EAugmentCategory::Active),Actives.Num());
        TestEqual(TEXT("Permanent evolution consumes no skill slot"),Query->GetSkillCount(),Skills.Num());
        TestTrue(TEXT("Skill order preserved"),Query->GetSkillIDs()==Skills);
        for(int32 I=0;I<Skills.Num();++I)TestTrue(TEXT("Slot order preserved"),Query->GetSkillAtSlot(I)==Skills[I]);
        TestTrue(TEXT("Invalid slot remains invalid"),Query->GetSkillAtSlot(-1)==static_cast<EAugmentID>(255));
    };
    Verify(Table);
    auto* Copy=World->SpawnActor<AMainPlayerCharacter>();auto* Restored=Copy->DispatchTable.Get();
    TestTrue(TEXT("Restore rebuilds all derived data"),Restored->RestoreAugments(History));Verify(Restored);
    const int32 Revision=Restored->GetAugmentRevision();TestFalse(TEXT("Second restore cannot duplicate cache"),Restored->RestoreAugments(History));
    TestEqual(TEXT("Rejected restore leaves revision unchanged"),Restored->GetAugmentRevision(),Revision);
    Restored->ResetForPool();TestEqual(TEXT("Pool reset clears stacks"),Restored->GetStackCount(EAugmentID::AttackUp),0);
    TestEqual(TEXT("Pool reset clears skills"),Restored->GetSkillCount(),0);TestEqual(TEXT("Pool reset clears species"),Restored->GetOwnedCount(EAugmentCategory::Passive),0);
    TestTrue(TEXT("Reuse after reset accepted"),Restored->ApplyAugment(EAugmentID::EffectRange));TestEqual(TEXT("New lifetime starts at one stack"),Restored->GetStackCount(EAugmentID::EffectRange),1);
    // Compare query cost on the same 300+ entry history. This is not an FPS benchmark.
    volatile int64 CachedSum=0,ReferenceSum=0;
    const double CachedStart=FPlatformTime::Seconds();
    for(int32 I=0;I<100000;++I)CachedSum=CachedSum+Table->GetStackCount(EAugmentID::AttackUp);
    const double CachedMs=(FPlatformTime::Seconds()-CachedStart)*1000;
    const double ReferenceStart=FPlatformTime::Seconds();
    for(int32 I=0;I<100000;++I){int32 Count=0;for(auto ID:History)Count+=ID==EAugmentID::AttackUp;ReferenceSum=ReferenceSum+Count;}
    const double ReferenceMs=(FPlatformTime::Seconds()-ReferenceStart)*1000;
    TestEqual(TEXT("Query checksums agree"),int64(CachedSum),int64(ReferenceSum));
    AddInfo(FString::Printf(TEXT("100000 stack lookups, %d entries: cached %.3f ms, reference linear scan %.3f ms"),History.Num(),CachedMs,ReferenceMs));
    GEngine->DestroyWorldContext(World);World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamAreaQueryTest,"DreamVeil.Optimization.AreaQueriesMatchBlueprintReferenceAndDedupe",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamAreaQueryTest::RunTest(const FString&)
{
    auto Values=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());World->BeginPlay();World->GetWorldSettings()->NotifyBeginPlay();
    TArray<AActor*> Actors;
    for(int32 I=0;I<40;++I)
    {
        auto* Actor=World->SpawnActor<AActor>();Actors.Add(Actor);
        for(int32 J=0;J<2;++J)
        {
            auto* Sphere=NewObject<USphereComponent>(Actor);if(J==0)Actor->SetRootComponent(Sphere);else Sphere->SetupAttachment(Actor->GetRootComponent());
            Sphere->SetSphereRadius(20);Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Sphere->SetCollisionObjectType(I%3==0?ECC_WorldDynamic:J==0?ECC_Pawn:MonsterCollision::Monster);
            Sphere->SetCollisionResponseToAllChannels(ECR_Overlap);Sphere->RegisterComponent();
        }
        Actor->SetActorLocation(FVector(30000+I*40,0,10000));
    }
    const TArray<TEnumAsByte<EObjectTypeQuery>> Types={UEngineTypes::ConvertToObjectType(ECC_Pawn),UEngineTypes::ConvertToObjectType(MonsterCollision::Monster)};
    for(float Radius:{30.f,220.f,2000.f})
    {
        const TArray<AActor*> Ignore={Actors[1],Actors[2]};TArray<AActor*> Reference,Actual;
        UKismetSystemLibrary::SphereOverlapActors(World,FVector(30000,0,10000),Radius,Types,AActor::StaticClass(),Ignore,Reference);
        UAugmentDamageLibrary::FindTargetsAtLocation(World,FVector(30000,0,10000),Radius,Ignore,Actual);
        TestEqual(TEXT("Same target count as original Blueprint query"),Actual.Num(),Reference.Num());
        TestEqual(TEXT("Multiple primitives never duplicate an actor"),TSet<AActor*>(Actual).Num(),Actual.Num());
        for(auto* Actor:Reference)TestTrue(TEXT("Original target retained"),Actual.Contains(Actor));
        for(auto* Actor:Ignore)TestFalse(TEXT("Ignored actors excluded"),Actual.Contains(Actor));
    }
    TArray<AActor*> Output=Actors;UAugmentDamageLibrary::FindTargetsAtLocation(nullptr,FVector::ZeroVector,100,{},Output);
    TestTrue(TEXT("Null context clears output"),Output.IsEmpty());
    GEngine->DestroyWorldContext(World);World->DestroyWorld(false);return true;
}
#endif
