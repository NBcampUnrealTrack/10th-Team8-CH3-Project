// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MonsterAIController.h"
#include "EliteMonsterAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Sight;

UCLASS()
class DREAMVEIL_API AEliteMonsterAIController : public AMonsterAIController
{
	GENERATED_BODY()
	
public:
	AEliteMonsterAIController();
	virtual void BeginPlay() override;

protected:
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Perception")
	TObjectPtr<UAIPerceptionComponent> AIPerception;

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

private:
	UFUNCTION()
	void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
};
