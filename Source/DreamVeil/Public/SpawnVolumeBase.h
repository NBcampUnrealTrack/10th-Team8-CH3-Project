// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpawnVolumeBase.generated.h"

class UBoxComponent;

UCLASS(Abstract)
class DREAMVEIL_API ASpawnVolumeBase : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ASpawnVolumeBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> SpawnArea;

	bool TryGetRandomNavLocation(FVector& OutLocation) const;

private:
	virtual void ExecuteSpawnActor();
};
