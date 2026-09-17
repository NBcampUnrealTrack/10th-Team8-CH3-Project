// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "MonsterAIController.generated.h"

class UAIPerceptionComonent;
class UAISenseConfig_Sight;
class UBehaviorTree;

UCLASS()
class DREAMVEIL_API AMonsterAIController : public AAIController
{
	GENERATED_BODY()
	
private:
	FTimerHandle RandomPatrolTime;

	UPROPERTY(EditAnywhere, Category="AI")
	float MoveRadius;

protected:
	virtual void OnPossess(APawn* InPawn) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|BehaviorTree")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;
public:
	AMonsterAIController();
	void StartBehaviorTree();
	virtual void BeginPlay() override;
};
