#pragma once

#include "CoreMinimal.h"
#include "AugmentTypes.generated.h"

//증강 번호 (액티브/패시브 통합)
UENUM(BlueprintType)
enum class EAugmentID : uint8
{
	//패시브
	AttackUp,
	DefenceUp,
	HealthUp,
	Berserker,
	ThornArmor,
	Vampire,
	Regeneration,

	//액티브
	SlowEnemy,
	AreaAttack,
	ContinuousAttack
};

//공격력 증강 수치
const float ATTACK_POWER_UP_AMOUNT = 5.0f;

//방어력 증강 수치
const float DEFENCE_POWER_UP_AMOUNT = 3.0f;

//체력 증강 수치
const float HEALTH_UP_AMOUNT = 20.0f;

//재생력 회복량
const float REGENERATION_HEAL_AMOUNT = 5.0f;

//재생력 발동 간격(초)
const float REGENERATION_INTERVAL = 2.0f;