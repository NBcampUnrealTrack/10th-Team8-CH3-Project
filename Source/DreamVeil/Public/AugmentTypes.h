#pragma once

#include "CoreMinimal.h"
#include "AugmentTypes.generated.h"

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

//공격력 증가량
const float ATTACK_POWER_UP_AMOUNT = 5.0f;

//방어력 증가량
const float DEFENCE_POWER_UP_AMOUNT = 3.0f;

//최대 체력 증가량
const float HEALTH_UP_AMOUNT = 20.0f;

//재생력 회복량
const float REGENERATION_HEAL_AMOUNT = 5.0f;

//재생력 회복 간격
const float REGENERATION_INTERVAL = 2.0f;

//흡혈 회복 비율
const float VAMPIRE_HEAL_RATIO = 0.1f;

//가시 갑옷 반사 비율
const float THORN_ARMOR_REFLECT_RATIO = 0.2f;

//광전사 발동 체력 비율
const float BERSERKER_THRESHOLD = 0.3f;

//광전사 공격력 배율
const float BERSERKER_MULTIPLIER = 1.5f;

//데미지 최소 보장치
const float MIN_DAMAGE = 1.0f;