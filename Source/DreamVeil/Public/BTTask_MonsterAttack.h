// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_MonsterAttack.generated.h"

class AMonsterBase;

UCLASS()
class DREAMVEIL_API UBTTask_MonsterAttack : public UBTTask_BlackboardBase
{
	GENERATED_BODY()
	
public:
    UBTTask_MonsterAttack();

protected:
    virtual EBTNodeResult::Type ExecuteTask(
        UBehaviorTreeComponent& OwnerComp,
        uint8* NodeMemory) override;

    virtual EBTNodeResult::Type AbortTask(
        UBehaviorTreeComponent& OwnerComp,
        uint8* NodeMemory) override;

private:
    void HandleAttackFinished(bool bSucceeded);
    void ClearBinding();

    TWeakObjectPtr<AMonsterBase> AttackingMonster;
    TWeakObjectPtr<UBehaviorTreeComponent> RunningBT;

    FDelegateHandle AttackFinishedHandle;
};
