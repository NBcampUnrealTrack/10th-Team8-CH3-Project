#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractableActorBase.generated.h"

class UBoxComponent;
class UStaticMeshComponent;

UCLASS(Abstract)
class DREAMVEIL_API AInteractableActorBase : public AActor
{
	GENERATED_BODY()

private:
	// 카메라 Line Trace를 감지할 영역
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> InteractionCollision;
	// 침대 또는 컴퓨터 외형
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ObjectMesh;
public:
	AInteractableActorBase();
	// 외형 메시 컴포넌트 반환
	UFUNCTION(BlueprintPure, Category = "Interaction")
	UStaticMeshComponent* GetObjectMesh() const;
	// 플레이어의 상호작용 처리
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Interaction")
	void Interact(AActor* Interactor);

	virtual void Interact_Implementation(AActor* Interactor);
};
