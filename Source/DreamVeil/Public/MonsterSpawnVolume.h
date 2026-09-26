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

	//한 마리 생성에 성공했는지 돌려줘서 실패한 예약은 게임모드가 다음 간격에 재시도함
	bool TrySpawnMonster();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster To Spawn")
	TArray<TSubclassOf<AMonsterBase>> BaseMonsters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster To Spawn")
	TArray<TSubclassOf<AMonsterBase>> EliteMonsters;

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
	//액터가 교체되거나 파괴되어도 무효 객체를 참조하지 않도록 약한 참조를 사용함
	TWeakObjectPtr<AMainPlayerCharacter> PlayerPawn;
};

//이전 구조 학습 메모: 예약과 타이머는 이제 GameMode가 담당함
//웨이브가 시작될 때 게임모드가 부름 이 볼륨에서 MonsterCount 마리를 SpawnInterval 간격으로 냄
//한꺼번에 쏟아내지 않고 끊어 내려고 타이머를 씀 실제로 한 마리를 내는 건 ExecuteSpawnActor가 함
//이미 내보내는 중에 또 불리면 남은 수에 더해져서 겹치지 않음
//아직 내보내지 못하고 남은 몬스터 수 웨이브 타이머가 하나씩 줄임
//남은 몬스터를 한 마리씩 내보내는 타이머 다 내면 스스로 멈춤
//타이머가 돌 때마다 한 마리 내보냄 남은 수가 0이 되면 타이머를 멈춤
