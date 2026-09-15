// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SpawnVolumeBase.h"
#include "MonsterSpawnVolume.generated.h"

class AMonsterBase;

UCLASS()
class DREAMVEIL_API AMonsterSpawnVolume : public ASpawnVolumeBase
{
	GENERATED_BODY()
	
public:
	UFUNCTION()
	void ExecuteSpawnActor() override; //호출을 이벤트로 하면 재밌을듯?

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster To Spawn")
	TArray<TObjectPtr<AMonsterBase>> BaseMonsters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster To Spawn")
	TArray<TObjectPtr<AMonsterBase>> EliteMonsters;
private:

};
