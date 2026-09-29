#include "BedActor.h"

#include "MainPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

ABedActor::ABedActor()
{
	// 침대 메시 검색
	static ConstructorHelpers::FObjectFinder<UStaticMesh>
		BedMeshFinder(TEXT("/Game/Maps/Asset/SM_Lobby_Bed.SM_Lobby_Bed"));
	// 메시 검색 실패 확인
	if (!BedMeshFinder.Succeeded())
	{
		UE_LOG(LogTemp, Error, TEXT("xxx"));
		return;
	}
	UStaticMeshComponent* ObjectMeshComponent =GetObjectMesh();
	// 메시 컴포넌트 확인
	if (!ObjectMeshComponent)
	{
		return;
	}
	// 침대 메시 적용
	ObjectMeshComponent->SetStaticMesh(BedMeshFinder.Object);
}

void ABedActor::Interact_Implementation(AActor* Interactor)
{
	Super::Interact_Implementation(Interactor);
	// 상호작용 대상 확인
	if (!Interactor)
	{
		return;
	}

	// 침대 BP 이벤트 실행
	OnBedInteracted(Interactor);

	//창을 띄울 컨트롤러를 기억해둠 예 버튼을 눌렀을 때 다시 찾지 않아도 되게
	CachedController = GetInteractingController(Interactor);

	if (!CachedController.IsValid())
	{
		return;
	}

	//확인창을 안 넣어뒀으면 묻는 단계를 건너뛰고 바로 고르게 함
	//여기서 그냥 돌아가면 침대를 눌러도 아무 일도 안 일어나서 고장처럼 보임
	if (!ConfirmWidgetClass)
	{
		OpenDreamSelect();
		return;
	}

	//로비는 싸우는 곳이 아니라 멈추지 않음 시간이 흘러도 위험하지 않고 멈추면 로비 연출까지 같이 멈춤
	//대신 OpenMenuWidget이 눌린 키를 버려주므로 W를 누른 채 열어도 캐릭터는 서 있음
	CachedController->OpenMenuWidget(ConfirmWidgetClass);
}

void ABedActor::OpenDreamSelect()
{
	if (!CachedController.IsValid() || !DreamSelectWidgetClass)
	{
		return;
	}

	//확인창을 따로 닫지 않는 이유 OpenMenuWidget이 열려 있던 창을 먼저 닫고 새로 엶
	//두 창이 겹쳐서 마우스가 먹통이 되는 것을 컨트롤러가 이미 막고 있음
	CachedController->OpenMenuWidget(DreamSelectWidgetClass);
}

void ABedActor::CloseBedMenu()
{
	if (!CachedController.IsValid())
	{
		return;
	}

	CachedController->CloseMenuWidget();
}

AMainPlayerController* ABedActor::GetInteractingController(AActor* Interactor) const
{
	//상호작용을 폰이 넘겼는지 컨트롤러가 넘겼는지 확실하지 않아서 둘 다 받아줌
	if (const APawn* InteractorPawn = Cast<APawn>(Interactor))
	{
		return Cast<AMainPlayerController>(InteractorPawn->GetController());
	}

	return Cast<AMainPlayerController>(Interactor);
}
