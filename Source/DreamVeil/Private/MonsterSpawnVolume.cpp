// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterSpawnVolume.h"
#include "Kismet/GameplayStatics.h"

/*
#include "MainPlayerCharacter.h"

AMonsterSpawnVolume::AMonsterSpawnVolume()
{
	MaxDifficultyLevel = 30;
	EliteMonsterMinRate = 5.0f;
	EliteMonsterMaxRate = 50.0f;
	DifficultyCurve = 1.0f;
}

void AMonsterSpawnVolume::ExecuteSpawnActor()
{
	FVector SpawnLocation;
	if (TryGetRandomNavLocation(SpawnLocation))
	{
		const float EliteSpawnRate = GetEliteRate();
		if (FMath::FRandRange(0.0f, 100.0f) <= EliteSpawnRate)
		{
			//엘리트 스폰
		}
		else
		{
			//일반 몬스터 스폰
		}
	}
	else
	{
		return;
	}
}

void AMonsterSpawnVolume::BeginPlay()
{
	Super::BeginPlay();

	PlayerPawn = Cast<AMainPlayerCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));

	MaxDifficultyLevel = FMath::Max(MaxDifficultyLevel, 2);

	EliteMonsterMinRate = FMath::Clamp(
		EliteMonsterMinRate,
		0.0f,
		99.0f
	);

	EliteMonsterMaxRate = FMath::Clamp(
		EliteMonsterMaxRate,
		EliteMonsterMinRate + 1.0f,
		100.0f
	);

	DifficultyCurve = FMath::Clamp(
		DifficultyCurve,
		0.1f,
		5.0f
	);
}

float AMonsterSpawnVolume::GetEliteRate()
{
	if (!PlayerPawn) return;
	int32 PlayerLevel = PlayerPawn->GetLevel();

	float LevelAlpha = FMath::Clamp(
		static_cast<float>(PlayerLevel - 1) / (MaxDifficultyLevel - 1),
		0.0f,
		1.0f
	);
	
	LevelAlpha = FMath::Pow(LevelAlpha, DifficultyCurve);

	return FMath::Lerp(EliteMonsterMinRate, EliteMonsterMaxRate, LevelAlpha);
}
*/