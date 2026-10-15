#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DreamCrowdTacticsSubsystem.h"
#include "MainPlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "UObject/UObjectHash.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamApproachAssetTest,"DreamVeil.Crowd.BehaviorTreeTracksMovingPlayerActor",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamApproachAssetTest::RunTest(const FString& Parameters)
{
    auto* Tree=LoadObject<UBehaviorTree>(nullptr,TEXT("/Game/Managers/AI/BT_Monster.BT_Monster"));
    if(!TestNotNull(TEXT("Real monster behavior tree exists"),Tree))return false;
    const auto* Blackboard=Tree->GetBlackboardAsset();
    if(!TestNotNull(TEXT("Real blackboard exists"),Blackboard))return false;
    const auto Key=Blackboard->GetKeyID(TEXT("PlayerActor"));
    TestTrue(TEXT("Approach key exists"),Key!=FBlackboard::InvalidKey);
    TestTrue(TEXT("Chase target is an actor"),Blackboard->GetKeyType(Key)==UBlackboardKeyType_Object::StaticClass());
    int32 Moves=0;
    ForEachObjectWithOuter(Tree,[&](UObject* Node){if(auto* Move=Cast<UBTTask_MoveTo>(Node))
    {++Moves;TestEqual(TEXT("MoveTo follows the moving player actor"),Move->GetSelectedBlackboardKey(),FName(TEXT("PlayerActor")));}},true);
    TestTrue(TEXT("At least one actual MoveTo was inspected"),Moves>0);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamCornerTest,"DreamVeil.Crowd.CornerRequiresTwoWallsAndStationaryPlayer",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamCornerTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Values=UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());World->BeginPlay();World->GetWorldSettings()->NotifyBeginPlay();
    auto* Player=World->SpawnActor<AMainPlayerCharacter>(FVector(100,100,10000),FRotator::ZeroRotator);
    Player->GetCharacterMovement()->DisableMovement();
    auto Wall=[&](FVector Position,FVector Extent)
    {
        auto* A=World->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(A);A->SetRootComponent(Box);
        Box->SetBoxExtent(Extent);Box->SetCollisionObjectType(ECC_WorldStatic);Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();A->SetActorLocation(Position);return A;
    };
    auto Advance=[&](float Seconds){for(float T=0;T<Seconds;T+=.01f){++GFrameCounter;World->Tick(LEVELTICK_All,.01f);}};
    auto* Tactics=World->GetSubsystem<UDreamCrowdTacticsSubsystem>();FVector Direction;
    auto* X=Wall(FVector(-20,100,10000),FVector(20,600,300));
    TestFalse(TEXT("No instant corner pressure"),Tactics->IsCornered(Player,Direction));Advance(2.1f);
    TestFalse(TEXT("One wall is not a corner"),Tactics->IsCornered(Player,Direction));
    auto* Y=Wall(FVector(100,-20,10000),FVector(600,20,300));Advance(.8f);
    TestTrue(TEXT("Stationary player at two perpendicular walls detected"),Tactics->IsCornered(Player,Direction));
    TestTrue(TEXT("Approach points face into the room"),Direction.X>.5f&&Direction.Y>.5f);
    Player->SetActorLocation(FVector(450,450,10000));
    TestFalse(TEXT("Leaving the corner immediately resets pressure"),Tactics->IsCornered(Player,Direction));
    Player->SetActorLocation(FVector(100,100,10000));Tactics->IsCornered(Player,Direction);
    Y->SetActorLocation(FVector(220,100,10000));Cast<UBoxComponent>(Y->GetRootComponent())->SetBoxExtent(FVector(20,600,300));Advance(2.1f);
    TestFalse(TEXT("Opposite corridor walls do not count as a corner"),Tactics->IsCornered(Player,Direction));
    X->Destroy();Y->Destroy();GEngine->DestroyWorldContext(World);World->DestroyWorld(false);return true;
}
#endif
