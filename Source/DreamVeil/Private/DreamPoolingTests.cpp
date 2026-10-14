#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DreamObjectPoolSubsystem.h"
#include "DreamPoolable.h"
#include "MonsterProjectile.h"
#include "MonsterBase.h"
#include "ThornSpikeEffect.h"
#include "BlackHoleZone.h"
#include "CombatStatsComponent.h"
#include "DispatchTableComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "TimerManager.h"

namespace DreamPoolTests
{
struct FTestWorld
{
    UWorld* World;
    FTestWorld()
    {
        UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
            .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        World->InitializeActorsForPlay(FURL());
        World->BeginPlay();
        World->GetWorldSettings()->NotifyBeginPlay();
    }
    ~FTestWorld() { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
    void Advance(float Seconds) { for (float Time = 0; Time < Seconds; Time += .05f) { ++GFrameCounter; World->Tick(LEVELTICK_All, .05f); } }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamPoolProjectileTest, "DreamVeil.Optimization.Pooling.ProjectileHitReuseAndTTL", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDreamPoolProjectileTest::RunTest(const FString& Parameters)
{
    DreamPoolTests::FTestWorld Test;
    UWorld* World = Test.World;
    AActor* Wall = World->SpawnActor<AActor>();
    const FTransform At(FRotator::ZeroRotator, FVector(0, 0, 10000));
    AMonsterProjectile* First = Cast<AMonsterProjectile>(UDreamObjectPoolSubsystem::Spawn(World, AMonsterProjectile::StaticClass(), At));
    if (!TestNotNull(TEXT("Projectile spawned"), First)) return false;
    UPrimitiveComponent* Collision = Cast<UPrimitiveComponent>(First->GetRootComponent());
    Collision->OnComponentHit.Broadcast(Collision, Wall, nullptr, FVector::ZeroVector, FHitResult());
    TestTrue(TEXT("Impact returns a live, hidden object"), IsValid(First) && First->IsHidden());
    TestFalse(TEXT("Inactive projectile collision disabled"), First->GetActorEnableCollision());
    Test.Advance(.1f);
    AMonsterProjectile* Second = Cast<AMonsterProjectile>(UDreamObjectPoolSubsystem::Spawn(World, AMonsterProjectile::StaticClass(), At));
    TestTrue(TEXT("Same projectile reused"), First == Second);
    TestTrue(TEXT("Reused projectile moves again"), Second->FindComponentByClass<UProjectileMovementComponent>()->Velocity.Size() > 0);
    Collision->OnComponentHit.Broadcast(Collision, Wall, nullptr, FVector::ZeroVector, FHitResult());
    TestTrue(TEXT("Hit guard reset on second life"), Second->IsHidden());
    Test.Advance(.1f);
    UDreamObjectPoolSubsystem::Spawn(World, AMonsterProjectile::StaticClass(), At);
    Test.Advance(5.2f);
    TestTrue(TEXT("TTL also returns the projectile"), First->IsHidden());
    TestEqual(TEXT("Repeated lives retain one projectile"), World->GetSubsystem<UDreamObjectPoolSubsystem>()->GetCachedActorCount(), 1);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamPoolTransientTest, "DreamVeil.Optimization.Pooling.ThornAndZoneReuse", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDreamPoolTransientTest::RunTest(const FString& Parameters)
{
    DreamPoolTests::FTestWorld Test;
    const FTransform At(FRotator::ZeroRotator, FVector(0, 0, 10000));
    AThornSpikeEffect* Thorn = Cast<AThornSpikeEffect>(UDreamObjectPoolSubsystem::Spawn(Test.World, AThornSpikeEffect::StaticClass(), At));
    Test.Advance(.8f);
    TestTrue(TEXT("Thorn lifetime returns it"), Thorn->IsHidden());
    AActor* ThornAgain = UDreamObjectPoolSubsystem::Spawn(Test.World, AThornSpikeEffect::StaticClass(), At);
    TestTrue(TEXT("Thorn reused"), ThornAgain == Thorn);
    ABlackHoleZone* Zone = Cast<ABlackHoleZone>(UDreamObjectPoolSubsystem::Spawn(Test.World, ABlackHoleZone::StaticClass(), At));
    Test.Advance(6.3f);
    TestTrue(TEXT("Expired empty zone returned"), Zone->IsHidden());
    AActor* ZoneAgain = UDreamObjectPoolSubsystem::Spawn(Test.World, ABlackHoleZone::StaticClass(), At);
    TestTrue(TEXT("Zone reused"), ZoneAgain == Zone);
    TestTrue(TEXT("Reuse has fresh warning timer"), ZoneAgain && ZoneAgain->IsActorTickEnabled() && !ZoneAgain->IsHidden());
    Test.Advance(6.3f);
    TestTrue(TEXT("Second zone lifetime expires independently"), Zone->IsHidden());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamPoolMonsterTest, "DreamVeil.Optimization.Pooling.MonsterFreshStatsAndTimers", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDreamPoolMonsterTest::RunTest(const FString& Parameters)
{
    DreamPoolTests::FTestWorld Test;
    const FTransform At(FRotator::ZeroRotator, FVector(0, 0, 10000));
    AMonsterBase* Monster = Cast<AMonsterBase>(UDreamObjectPoolSubsystem::Spawn(Test.World, AMonsterBase::StaticClass(), At));
    const float OriginalAttack = Monster->MonsterCombatStats->GetAttackPower();
    Monster->MonsterDispatchTable->ApplyAugment(EAugmentID::AttackUp);
    TestTrue(TEXT("First life augment changes attack"), Monster->MonsterCombatStats->GetAttackPower() > OriginalAttack);
    Monster->MonsterCombatStats->SetCurrentHealth(0);
    Test.Advance(1.2f);
    TestTrue(TEXT("Death animation cleanup returns monster"), IsValid(Monster) && Monster->IsHidden());
    TestFalse(TEXT("Cached movement sleeps"), Monster->GetCharacterMovement()->IsComponentTickEnabled());
    Test.Advance(5.2f);
    AMonsterBase* Again = Cast<AMonsterBase>(UDreamObjectPoolSubsystem::Spawn(Test.World, AMonsterBase::StaticClass(), At));
    TestTrue(TEXT("Dead monster reused"), Again == Monster);
    TestFalse(TEXT("New life alive"), Again->MonsterCombatStats->IsDead());
    TestEqual(TEXT("Previous augment does not stack onto new life"), Again->MonsterCombatStats->GetAttackPower(), OriginalAttack);
    TestEqual(TEXT("Augment selection history cleared"), Again->MonsterDispatchTable->GetAugmentHistory().Num(), 0);
    TestTrue(TEXT("Ragdoll reset"), !Again->IsRagdoll() && Again->GetActorEnableCollision());
    TestTrue(TEXT("Lifetime generation advances"), UDreamObjectPoolSubsystem::Generation(Again) >= 2);
    Test.Advance(1.2f);
    TestFalse(TEXT("Old death timer cannot remove new life"), Again->IsHidden());
    return true;
}
#endif
