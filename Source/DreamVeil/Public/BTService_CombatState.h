#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Services/BTService_BlackboardBase.h"
#include "BTService_CombatState.generated.h"

UCLASS()
class DREAMVEIL_API UBTService_CombatState : public UBTService_BlackboardBase
{
	GENERATED_BODY()
	
public:
	UBTService_CombatState();
	
	void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

};
