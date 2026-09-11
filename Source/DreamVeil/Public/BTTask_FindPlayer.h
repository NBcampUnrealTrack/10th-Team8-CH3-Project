<<<<<<< Updated upstream
// Fill out your copyright notice in the Description page of Project Settings.

=======
>>>>>>> Stashed changes
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_FindPlayer.generated.h"

<<<<<<< Updated upstream
/**
 * 
 */
=======
>>>>>>> Stashed changes
UCLASS()
class DREAMVEIL_API UBTTask_FindPlayer : public UBTTask_BlackboardBase
{
	GENERATED_BODY()
	
<<<<<<< Updated upstream
public:
	UBTTask_FindPlayer();

	EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory);

=======
protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory);
public:
	UBTTask_FindPlayer();
>>>>>>> Stashed changes
};
