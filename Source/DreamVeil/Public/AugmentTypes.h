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
	Knockback,
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
//AREA_ATTACK_DRAW_DEBUG와 같은 용도 문제가 생겼을 때만 켤 것
const bool THORN_ARMOR_DRAW_DEBUG = false;

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

//넉백 세기 총에 맞은 적이 뒤로 밀리는 속도 cm/s
//질량을 무시하고 속도를 직접 더하므로(bVelocityChange = true) 이 값이 곧 초기 속도가 됨
//걷기 마찰로 금방 멈춰서 실제로 밀리는 거리는 훨씬 짧음 대략 속도 나누기 마찰계수만큼 감
//보스 돌진이 쓰는 600 + 위로 300보다 큰데도 덜 날아가는 이유 위 성분이 없어 땅에 붙은 채로 마찰을 받기 때문
//띄우지 않는 이유 공중에 뜨면 마찰이 없어서 거리가 제멋대로 늘어남 800이면 한 발자국 반쯤
const float KNOCKBACK_IMPULSE = 800.0f;

//범위 공격 반경 총알이 맞은 지점 기준
//눈에 보이는 폭발 크기와 같아야 함 불덩이 밖의 적이 죽거나 안에 있는 적이 멀쩡하면 규칙이 안 읽힘
//무기의 ExplosionEffectBaseRadius와 짝으로 맞춰야 함 이 값을 바꾸면 이펙트도 같이 커지고 줄어듦
const float AREA_ATTACK_RADIUS = 200.0f;

//범위 공격이 터진 자리를 잠깐 그려서 반경과 걸린 대상 수를 눈으로 확인
//켜면 빨간 구체와 적중 인원 로그가 같이 나옴 수치를 다시 만질 때만 켤 것
const bool AREA_ATTACK_DRAW_DEBUG = false;

//범위 공격 데미지 비율 총 데미지 기준 폭발 한가운데에 있을 때의 값
//1이 아니라 0.7인 이유 직접 조준해서 맞힌 적은 1을 온전히 받으므로
//폭발은 그보다 낮아야 조준할 이유가 남음 노린 것과 휩쓸린 것의 값이 같으면 안 됨
const float AREA_ATTACK_DAMAGE_RATIO = 0.7f;

//폭발 가장자리에서도 남는 최소 비율 0이면 가장자리는 아예 안 아픔
//0으로 두면 0.7 x 감쇠가 평균 0.35라 균일하게 0.5를 주던 때보다 오히려 약해짐
//바닥을 깔되 낮게 두어서 한가운데에 맞히는 것과 스치는 것의 차이를 크게 남김
const float AREA_ATTACK_MIN_FALLOFF = 0.3f;

//지속 공격 화염 데미지 간격
const float CONTINUOUS_ATTACK_INTERVAL = 1.0f;

//지속 공격 화염 지속 시간 다시 맞으면 시간만 처음부터 다시
const float CONTINUOUS_ATTACK_DURATION = 3.0f;

//지속 공격 틱당 데미지 비율 총 데미지 기준 대상 방어력을 무시하므로 낮게 잡음
const float CONTINUOUS_ATTACK_DAMAGE_RATIO = 0.1f;
