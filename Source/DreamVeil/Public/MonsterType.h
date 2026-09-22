// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MonsterType.generated.h"

UENUM(BlueprintType)
enum class EMonsterAttackType : uint8
{
	Melee,	// 근거리
	Ranged,	// 원거리
	Hybrid	// 근거리, 원거리 둘 다 사용
	//기본적으로 근접, 원거리, 둘 다 사용하는 개체 나누기
};

// 엘리트 몬스터 타입 정의. 일단 네이밍은 간단하게 Base로 시작. 추가할때 논의
UENUM(BlueprintType)
enum class EEliteMonsterType : uint8
{
	Elite1,
	Elite2,
	Elite3
};