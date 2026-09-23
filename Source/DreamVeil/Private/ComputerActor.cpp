#include "ComputerActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AComputerActor::AComputerActor()
{
	// 컴퓨터 Static Mesh 검색
	static ConstructorHelpers::FObjectFinder<UStaticMesh>
		ComputerMeshFinder(TEXT("/Game/Maps/Asset/SM_Lobby_Computer.SM_Lobby_Computer"));
	// 메시를 찾지 못하면 종료
	if (!ComputerMeshFinder.Succeeded())
	{
		UE_LOG(LogTemp, Error, TEXT("xxx"));
		return;
	}
	UStaticMeshComponent* ObjectMeshComponent =GetObjectMesh();
	// 메시 컴포넌트가 없으면 종료
	if (!ObjectMeshComponent)
	{
		return;
	}
	// 컴퓨터 메시 적용
	ObjectMeshComponent->SetStaticMesh(ComputerMeshFinder.Object);
}
void AComputerActor::Interact_Implementation(AActor* Interactor)
{
	Super::Interact_Implementation(Interactor);
	// 상호작용 대상이 없으면 종료
	if (!Interactor)
	{
		return;
	}
	// 상호작용 성공 확인
	UE_LOG(LogTemp, Log, TEXT("ooo"));
	// 컴퓨터 BP 이벤트 실행
	OnComputerInteracted(Interactor);
}