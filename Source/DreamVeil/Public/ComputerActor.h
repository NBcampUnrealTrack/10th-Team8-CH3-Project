#pragma once

#include "CoreMinimal.h"
#include "InteractableActorBase.h"
#include "ComputerActor.generated.h"

UCLASS()
class DREAMVEIL_API AComputerActor : public AInteractableActorBase
{
	GENERATED_BODY()

protected:
	// BP에서 상점 기능을 연결할 이벤트
	UFUNCTION(BlueprintImplementableEvent, Category = "Interaction")
	void OnComputerInteracted(AActor* Interactor);
public:
	AComputerActor();
	// 컴퓨터 상호작용 처리
	virtual void Interact_Implementation(AActor* Interactor) override;
};