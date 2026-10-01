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
//5에서 3으로 내림 반복 획득이 되는 증강이라 한 번에 주는 양이 크면 공격력만 쌓는 것이 늘 정답이 됨
const float ATTACK_POWER_UP_AMOUNT = 3.0f;

//방어력 증가량
const float DEFENCE_POWER_UP_AMOUNT = 3.0f;

//최대 체력 증가량
const float HEALTH_UP_AMOUNT = 20.0f;

//최대 스태미나 증가량 달리기가 초당 25를 먹으므로 한 번에 약 0.4초 더 뛸 수 있음
//25에서 10으로 내림 달리기 보조 증강이 공방체 증가와 같은 급으로 체감되면 안 됨
const float STAMINA_UP_AMOUNT = 10.0f;

//재생력 회복 비율 회복량 = (최대 체력 - 현재 체력) * 이 값
//고정 수치가 아니라 잃은 체력을 보는 이유 체력 증가 증강으로 최대 체력이 커지면 회복량도 같이 커져야 하고
//체력이 꽉 찬 상태에서는 회복량이 0이 되어 전투 중에만 뜻이 생기게 하려는 것
//최대 체력까지 완전히 채우지는 못함 남은 양에 비례해서 줄기 때문에 끝에 가면 거의 0이 됨
const float REGENERATION_LOST_HEALTH_RATIO = 0.01f;

//재생력 회복 간격
const float REGENERATION_INTERVAL = 2.0f;

//흡혈 회복 비율 회복량 = 입힌 피해 * 이 값
//0.1에서 0.005로 내림 연사 무기에 폭발탄까지 붙으면 한 번 쏠 때 들어가는 타격 수가 많아서
//비율이 조금만 높아도 맞으면서 계속 차오름
const float VAMPIRE_HEAL_RATIO = 0.005f;

//가시 갑옷 반사량 계수 반사 데미지 = 방어력 * 이 값
//받은 데미지 비율이 아니라 방어력을 보는 이유 약한 공격을 여러 번 맞을 때도 반사가 일정하고
//방어력 증강과 최후의 요새가 가시 갑옷과 같이 세지는 조합이 생김
//방어력이 0이면 반사도 0 플레이어 기본 방어력이 0이라 방어력 증강을 먼저 먹어야 뜻이 생김
//최소 반사량을 따로 두지 않은 이유 가시 갑옷을 방어 빌드 전용 증강으로 두기로 함
//방어력 증가와 최후의 요새를 같이 모으면 세지고 공격 빌드에서는 안 고르는 것이 맞는 선택이 되게 함
const float THORN_ARMOR_DEFENCE_RATIO = 1.0f;

//가시 갑옷이 어디서 멈췄는지 로그로 남김 반사가 계산됐는지 연출까지 갔는지 한눈에 보임
//AREA_ATTACK_DRAW_DEBUG와 같은 용도 문제가 생겼을 때만 켤 것
const bool THORN_ARMOR_DRAW_DEBUG = false;

//광전사 발동 체력 비율
//0.3에서 0.4로 올림 최후의 요새(0.3)보다 먼저 켜져서 둘을 같이 모으면 단계가 생김
const float BERSERKER_THRESHOLD = 0.4f;

//광전사 공격력 배율
const float BERSERKER_MULTIPLIER = 1.5f;

//최후의 요새 발동 체력 비율
const float LAST_FORTRESS_THRESHOLD = 0.3f;

//최후의 요새 방어력 배율
const float LAST_FORTRESS_MULTIPLIER = 1.5f;

//방어력 피해 감소 상수 받는 피해 = 피해 * 이 값 / (이 값 + 방어력)
//빼기가 아니라 나누기로 바꾼 이유 빼기는 방어력이 피해량을 넘는 순간 모든 공격이 최소 피해로 똑같아져서
//그 뒤로는 방어력을 더 올릴 이유가 사라지고 반대로 아주 큰 공격에는 거의 효과가 없음
//나누기는 방어력 1당 늘어나는 실질 체력이 늘 같아서(1 나누기 이 값) 얼마까지 올려도 값이 붙음
//30으로 잡은 이유 방어력이 이 값과 같아질 때 받는 피해가 정확히 절반이 됨
//방어력 증강 한 개가 3이니 10개를 모으면 절반 거기에 최후의 요새까지 켜지면 60% 감소
//몬스터 방어력 1은 3% 감소라 예전(피해 빼기 1)과 거의 같고 플레이어 기본 방어력 0은 감소가 없어 전과 완전히 같음
const float DEFENCE_REDUCTION_CONSTANT = 30.0f;

//데미지 최소 보장치
//나누기 방식은 결과가 0이 되지 않지만 아주 약한 공격이 소수점 피해로 묻히는 것은 막아야 함
const float MIN_DAMAGE = 1.0f;

//넉백 세기 총에 맞은 적이 뒤로 밀리는 속도 cm/s
//질량을 무시하고 속도를 직접 더하므로(bVelocityChange = true) 이 값이 곧 초기 속도가 됨
//걷기 마찰로 금방 멈춰서 실제로 밀리는 거리는 훨씬 짧음 대략 속도 나누기 마찰계수만큼 감
//보스 돌진이 쓰는 600 + 위로 300보다 큰데도 덜 날아가는 이유 위 성분이 없어 땅에 붙은 채로 마찰을 받기 때문
//띄우지 않는 이유 공중에 뜨면 마찰이 없어서 거리가 제멋대로 늘어남 800이면 한 발자국 반쯤
const float KNOCKBACK_IMPULSE = 800.0f;

//범위 공격 반경 총알이 맞은 지점 기준
//무기의 ExplosionEffectBaseRadius와 짝으로 맞춰야 함 이 값을 바꾸면 이펙트도 같이 커지고 줄어듦
//보이는 불덩이는 아래 AREA_ATTACK_EFFECT_SCALE만큼 더 작게 그림 피해가 들어가는 범위는 여기 적힌 값 그대로임
const float AREA_ATTACK_RADIUS = 200.0f;

//보이는 폭발 이펙트를 피해 반경보다 작게 그리는 비율 1이면 피해 반경과 똑같은 크기
//피해 범위와 일부러 어긋나게 둔 이유 불덩이가 화면을 덮으면 폭발 직후에 적이 어디 있는지가 안 보임
//전투 중에 앞이 안 보이는 쪽이 범위를 눈으로 못 재는 쪽보다 훨씬 손해라서 보이는 크기만 줄임
//너무 낮추면 어디까지 맞는지 못 읽으니 피해가 센 한가운데만 보여주는 정도로 둠
//디버그 구체(AREA_ATTACK_DRAW_DEBUG)는 이 값을 안 쓰고 진짜 피해 반경을 그대로 그림
const float AREA_ATTACK_EFFECT_SCALE = 0.7f;

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
