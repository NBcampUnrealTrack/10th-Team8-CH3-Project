
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_FindPlayer.generated.h"

UCLASS()
class DREAMVEIL_API UBTTask_FindPlayer : public UBTTask_BlackboardBase
{
	GENERATED_BODY()
protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory);
public:
	UBTTask_FindPlayer();
};
