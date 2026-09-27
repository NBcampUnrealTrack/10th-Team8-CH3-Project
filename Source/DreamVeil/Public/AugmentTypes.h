//지속 공격 독 데미지 간격
//지속 공격 독 지속 시간 다시 맞으면 시간만 처음부터 다시
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
	//최대 스태미나 증가 스태미나는 플레이어만 쓰므로 스탯 컴포넌트가 아니라 플레이어 캐릭터 값을 올림
	StaminaUp,
	Berserker,
	ThornArmor,
	Vampire,
	Regeneration,
	LastFortress,
	//액티브
	SlowEnemy,
	AreaAttack,
	ContinuousAttack
};

//증강 분류 보상 UI 표시나 나중에 무기 증강을 나눌 때 씀
UENUM(BlueprintType)
enum class EAugmentCategory : uint8
{
	//패시브 스탯 계열
	Passive,
	//액티브 발동 계열
	Active,
	//무기 계열 아직 담당 스킬이 없고 WeaponComponent가 생기면 연결
	Weapon
};

//보상 UI에 띄울 증강 선택지 개수
const int32 AUGMENT_CHOICE_COUNT = 3;

//공격력 증가량
const float ATTACK_POWER_UP_AMOUNT = 5.0f;

//방어력 증가량
const float DEFENCE_POWER_UP_AMOUNT = 3.0f;

//최대 체력 증가량
const float HEALTH_UP_AMOUNT = 20.0f;

//최대 스태미나 증가량 기본 최대치 100 기준 한 번에 약 1초 더 뛸 수 있음
const float STAMINA_UP_AMOUNT = 25.0f;

//재생력 회복량
const float REGENERATION_HEAL_AMOUNT = 5.0f;

//재생력 회복 간격
const float REGENERATION_INTERVAL = 2.0f;

//흡혈 회복 비율
const float VAMPIRE_HEAL_RATIO = 0.1f;

//가시 갑옷 반사 비율
const float THORN_ARMOR_REFLECT_RATIO = 0.2f;

//가시 갑옷이 어디서 멈췄는지 로그로 남김 반사가 계산됐는지 연출까지 갔는지 한눈에 보임
//AREA_ATTACK_DRAW_DEBUG와 같은 용도 출시 전에 false로 둘 것
const bool THORN_ARMOR_DRAW_DEBUG = true;

//광전사 발동 체력 비율
const float BERSERKER_THRESHOLD = 0.3f;

//광전사 공격력 배율
const float BERSERKER_MULTIPLIER = 1.5f;

//최후의 요새 발동 체력 비율
const float LAST_FORTRESS_THRESHOLD = 0.3f;

//최후의 요새 방어력 배율
const float LAST_FORTRESS_MULTIPLIER = 1.5f;

//데미지 최소 보장치
const float MIN_DAMAGE = 1.0f;

//감속 지속 시간 총에 맞은 대상 기준 다시 맞으면 시간만 처음부터 다시
const float SLOW_ENEMY_DURATION = 3.0f;

//감속 이동 속도 배율 다시 맞아도 겹쳐서 더 느려지지 않음
const float SLOW_ENEMY_RATIO = 0.5f;

//범위 공격 반경 총알이 맞은 지점 기준
//4m에서 8m로 넓힘 4m는 몬스터가 서로 조금만 떨어져도 아무도 안 걸려서 터지는지도 몰랐음
const float AREA_ATTACK_RADIUS = 800.0f;

//범위 공격이 터진 자리를 잠깐 그려서 반경과 걸린 대상 수를 눈으로 확인
//WeaponBase의 bDrawDebugTrace와 같은 용도 출시 전에 false로 둘 것
const bool AREA_ATTACK_DRAW_DEBUG = true;

//범위 공격 데미지 비율 총 데미지 기준 주변 대상 각자의 방어력은 따로 빠짐
const float AREA_ATTACK_DAMAGE_RATIO = 1.0f;

//지속 공격 화염 데미지 간격
const float CONTINUOUS_ATTACK_INTERVAL = 1.0f;

//지속 공격 화염 지속 시간 다시 맞으면 시간만 처음부터 다시
const float CONTINUOUS_ATTACK_DURATION = 3.0f;

//지속 공격 틱당 데미지 비율 총 데미지 기준 대상 방어력을 무시하므로 낮게 잡음
const float CONTINUOUS_ATTACK_DAMAGE_RATIO = 0.1f;
