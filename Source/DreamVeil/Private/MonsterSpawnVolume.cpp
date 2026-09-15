// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterSpawnVolume.h"

void AMonsterSpawnVolume::ExecuteSpawnActor()
{
	FVector SpawnLocation;
	if (TryGetRandomNavLocation(SpawnLocation))
	{
		
	}
	else
	{
		return;
	}
}
