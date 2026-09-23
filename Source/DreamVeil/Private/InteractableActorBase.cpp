#include "InteractableActorBase.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

AInteractableActorBase::AInteractableActorBase()
{
	// Tick 사용 중지
	PrimaryActorTick.bCanEverTick = false;
	// 상호작용 충돌 영역 생성
	InteractionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionCollision"));
	SetRootComponent(InteractionCollision);
	// Line Trace 검사만 허용
	InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	// 움직일 수 있는 오브젝트로 설정
	InteractionCollision->SetCollisionObjectType(ECC_WorldDynamic);
	// 기본적으로 모든 채널 무시
	InteractionCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	// Visibility Line Trace만 차단
	InteractionCollision->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
	// 기본 상호작용 박스 크기
	InteractionCollision->SetBoxExtent(FVector(75.0f));
	// 내비게이션 계산에서 제외
	InteractionCollision->SetCanEverAffectNavigation(false);
	// 외형 메시 컴포넌트 생성
	ObjectMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ObjectMesh"));
	ObjectMesh->SetupAttachment(InteractionCollision);
}
UStaticMeshComponent*AInteractableActorBase::GetObjectMesh() const
{
	return ObjectMesh;
}
void AInteractableActorBase::Interact_Implementation(AActor* Interactor)
{
}
