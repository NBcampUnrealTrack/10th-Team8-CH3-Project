#include "BedActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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
	UE_LOG(LogTemp, Log, TEXT("ooo"));
	// 침대 BP 이벤트 실행
	OnBedInteracted(Interactor);
}

