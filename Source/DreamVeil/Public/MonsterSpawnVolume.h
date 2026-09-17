// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SpawnVolumeBase.h"
#include "MonsterSpawnVolume.generated.h"

class AMainPlayerCharacter;
class AMonsterBase;

UCLASS()
class DREAMVEIL_API AMonsterSpawnVolume : public ASpawnVolumeBase
{
	GENERATED_BODY()
	
public:
	AMonsterSpawnVolume();

	UFUNCTION()
	void ExecuteSpawnActor() override; //호출을 이벤트로 하면 재밌을듯?

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster To Spawn")
	TArray<TObjectPtr<AMonsterBase>> BaseMonsters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster To Spawn")
	TArray<TObjectPtr<AMonsterBase>> EliteMonsters;

	// 최대 엘리토 몬스터가 스폰되는 레벨. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster To Spawn")
	int32 MaxDifficultyLevel;

	// 엘리토 몬스터가 스폰되는 최소확률, 기본값 5%. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster To Spawn")
	float EliteMonsterMinRate;

	// 레벨에 따른 엘리트 몬스터 최대 스폰율. %로 되기에 100 이상이면 모두 엘리트 몬스터로 나올 가능성이 있음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster To Spawn")
	float EliteMonsterMaxRate;

	// 최대 스폰율까지 오를 수 있는 상승 곡선 경사, 곧 난이도가 얼마나 빨리 어려워지는지를 설정할 수 있음.
	// 0.1 ~ 3.0을 추천. 
	// 0.1 = 경사가 매우 가파르게 오름. 초반에 엄청 빡세짐
	// 3.0 = 경사가 매우 완만하게 오름. 초반이 매우 쉬워짐
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster To Spawn")
	float DifficultyCurve;

private:
	float GetEliteRate();
	TObjectPtr<AMainPlayerCharacter> PlayerPawn;
};
