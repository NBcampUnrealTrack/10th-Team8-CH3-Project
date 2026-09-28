// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpawnVolumeBase.generated.h"

class UArrowComponent;
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

	//뷰포트에서 이 볼륨의 앞쪽이 어디인지 보여주는 화살표 에디터에서만 보임
	//몬스터가 이 방향을 보고 나오므로 배치할 때 화살표를 문 바깥쪽으로 돌려둘 것
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UArrowComponent> SpawnDirection;

	bool TryGetRandomNavLocation(FVector& OutLocation) const;

private:
	virtual void ExecuteSpawnActor();
};
