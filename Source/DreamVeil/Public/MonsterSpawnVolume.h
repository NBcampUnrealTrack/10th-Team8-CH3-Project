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

	//웨이브가 시작될 때 게임모드가 부름 이 볼륨에서 MonsterCount 마리를 SpawnInterval 간격으로 냄
	//한꺼번에 쏟아내지 않고 끊어 내려고 타이머를 씀 실제로 한 마리를 내는 건 ExecuteSpawnActor가 함
	//이미 내보내는 중에 또 불리면 남은 수에 더해져서 겹치지 않음
	UFUNCTION(BlueprintCallable, Category = "Monster To Spawn")
	void SpawnWave(int32 MonsterCount, float SpawnInterval);

	//게임모드가 일정 주기마다 부름 이 볼륨이 한 번에 몇 마리를 낼지는 볼륨이 정함
	//게임모드가 수를 정하지 않는 이유 좁은 방과 넓은 마당에 같은 수를 내면 한쪽은 텅 비고 한쪽은 꽉 막힘
	//볼륨마다 MonstersPerSpawnTick을 다르게 두면 맵을 만들면서 밀도를 조절할 수 있음
	UFUNCTION(BlueprintCallable, Category = "Monster To Spawn")
	void SpawnTick();

protected:
	virtual void BeginPlay() override;

	//낼 수 있는 잡몹 종류 이 중에서 하나를 뽑아서 냄 비워두면 잡몹이 안 나옴
	//인스턴스가 아니라 클래스를 들고 있는 이유 SpawnActor는 클래스를 받음
	//레벨에 미리 놓아둔 액터를 가리키게 하면 그 한 마리만 존재해서 여러 마리를 낼 수 없음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster To Spawn")
	TArray<TSubclassOf<AMonsterBase>> BaseMonsters;

	//낼 수 있는 엘리트 종류 비워두면 확률에 당첨돼도 잡몹이 나옴
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster To Spawn")
	TArray<TSubclassOf<AMonsterBase>> EliteMonsters;

	//이 볼륨이 한 번 스폰 신호를 받을 때 낼 몬스터 수
	//0으로 두면 이 볼륨은 아무것도 안 냄 장식용 볼륨이나 보스방에서 씀
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster To Spawn")
	int32 MonstersPerSpawnTick = 1;

	//한 번에 여러 마리를 낼 때 마리 사이 간격 초
	//한 프레임에 다 쏟으면 같은 자리에 겹쳐서 나와 서로 밀어내느라 튕김
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster To Spawn")
	float SpawnTickGap = 0.3f;

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
	//아직 내보내지 못하고 남은 몬스터 수 웨이브 타이머가 하나씩 줄임
	int32 PendingSpawnCount = 0;

	//남은 몬스터를 한 마리씩 내보내는 타이머 다 내면 스스로 멈춤
	FTimerHandle WaveSpawnTimerHandle;

	//타이머가 돌 때마다 한 마리 내보냄 남은 수가 0이 되면 타이머를 멈춤
	void SpawnOneFromWave();

	float GetEliteRate();

	//고른 목록에서 하나를 뽑아 땅 위에 냄 목록이 비었거나 스폰에 실패하면 nullptr
	//잡몹과 엘리트가 같은 절차를 타게 하려고 따로 뺌
	AMonsterBase* SpawnOneMonster(const TArray<TSubclassOf<AMonsterBase>>& MonsterClasses, const FVector& SpawnLocation);

	TObjectPtr<AMainPlayerCharacter> PlayerPawn;
};
