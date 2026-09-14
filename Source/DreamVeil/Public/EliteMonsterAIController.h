// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EliteMonsterAIController.generated.h"

UCLASS()
class DREAMVEIL_API AEliteMonsterAIController : public AAIController
{
	GENERATED_BODY()
	
private:
	FTimerHandle RandomPatrolTime;

	UPROPERTY(EditAnywhere, Category = "AI")
	float MoveRadius;
};
