#pragma once

#include "CoreMinimal.h"
#include "InteractableActorBase.h"
#include "BedActor.generated.h"

UCLASS()
class DREAMVEIL_API ABedActor : public AInteractableActorBase
{
	GENERATED_BODY()

protected:
	// BP에서 스테이지 기능을 연결할 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
	void OnBedInteracted(AActor* Interactor);
public:
	ABedActor();
	// 침대 상호작용 처리
	virtual void Interact_Implementation(AActor* Interactor) override;
};
