#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "MainPlayerCharacter.h"
#include "InteractableActorBase.h"
#include "Components/BoxComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamInteractionTargetTest,"DreamVeil.Interaction.SharedTargetDistanceNearestAndWalls",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamInteractionTargetTest::RunTest(const FString& Parameters)
{
    UWorld::InitializationValues Values=UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    World->InitializeActorsForPlay(FURL());World->BeginPlay();World->GetWorldSettings()->NotifyBeginPlay();
    auto* Player=World->SpawnActor<AMainPlayerCharacter>(FVector(0,0,10000),FRotator::ZeroRotator);Player->GetCharacterMovement()->DisableMovement();
    auto* BedClass=LoadClass<AInteractableActorBase>(nullptr,TEXT("/Game/Maps/BP/Object/BP_BedActor.BP_BedActor_C"));
    auto* ComputerClass=LoadClass<AInteractableActorBase>(nullptr,TEXT("/Game/Maps/BP/Object/BP_ComputerActor.BP_ComputerActor_C"));
    if(TestNotNull(TEXT("Real bed class"),BedClass)&&TestNotNull(TEXT("Real computer class"),ComputerClass))
    {
        FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Bed=World->SpawnActor<AInteractableActorBase>(BedClass,FVector(3000,0,10000),FRotator::ZeroRotator,Params);
        auto* Computer=World->SpawnActor<AInteractableActorBase>(ComputerClass,FVector(4000,0,10000),FRotator::ZeroRotator,Params);
        TestNull(TEXT("No prompt target outside interaction range"),Player->FindInteractionTarget());
        Bed->SetActorLocation(FVector(210,0,10000));
        TestTrue(TEXT("Near bed is usable"),Player->FindInteractionTarget()==Bed);
        Computer->SetActorLocation(FVector(110,0,10000));
        TestTrue(TEXT("Prompt and F will choose closest usable furniture"),Player->FindInteractionTarget()==Computer);
        auto* Wall=World->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Wall);Wall->SetRootComponent(Box);
        Box->SetBoxExtent(FVector(10,600,300));Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionObjectType(ECC_WorldStatic);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();Wall->SetActorLocation(FVector(65,0,10000));
        TestNull(TEXT("A wall blocks both prompt and interaction"),Player->FindInteractionTarget());
        Wall->Destroy();Computer->SetActorLocation(FVector(4000,0,10000));Bed->SetActorLocation(FVector(3000,0,10000));
        TestNull(TEXT("Moving away removes target again"),Player->FindInteractionTarget());
    }
    GEngine->DestroyWorldContext(World);World->DestroyWorld(false);return true;
}
#endif
