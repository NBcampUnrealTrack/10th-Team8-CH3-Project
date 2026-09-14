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

//증강 분류 4계층이 이걸 보고 어느 컴포넌트로 넘길지 결정
UENUM(BlueprintType)
enum class EAugmentCategory : uint8
{
	//패시브 스탯 계열
	Passive,
	//액티브 발동 계열
	Active,
	//무기 계열 아직 담당 컴포넌트가 없고 5계층에서 연결할 자리만 잡아둠
	Weapon
};

//증강 하나의 정보 4계층 풀에 담기는 단위
USTRUCT(BlueprintType)
struct FAugmentData
{
	GENERATED_BODY()

	//증강 번호
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
	EAugmentID AugmentID = EAugmentID::AttackUp;

	//증강 분류
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
	EAugmentCategory Category = EAugmentCategory::Passive;

	//뽑기 가중치 클수록 잘 나옴
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
	float Weight = 1.0f;

	//반복 획득 가능 여부 false면 한 번 뽑힌 뒤 풀에서 제거
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
	bool bRepeatable = false;

	//보상 UI에 띄울 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
	FText DisplayName;

	//기본 생성자
	FAugmentData() {}

	//값을 채워 만드는 생성자
	FAugmentData(EAugmentID InAugmentID, EAugmentCategory InCategory, float InWeight, bool bInRepeatable)
		: AugmentID(InAugmentID)
		, Category(InCategory)
		, Weight(InWeight)
		, bRepeatable(bInRepeatable)
	{
	}
};

//번호만 보고 분류를 알아냄 풀에 없는 증강을 직접 실행할 때 씀
inline EAugmentCategory GetAugmentCategory(EAugmentID AugmentID)
{
	switch (AugmentID)
	{
	case EAugmentID::SlowEnemy:
	case EAugmentID::AreaAttack:
	case EAugmentID::ContinuousAttack:
		return EAugmentCategory::Active;
	default:
		return EAugmentCategory::Passive;
	}
}

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

//속도 저하 탐지 반경
const float SLOW_ENEMY_RADIUS = 600.0f;

//속도 저하 발동 간격
const float SLOW_ENEMY_INTERVAL = 5.0f;

//속도 저하 지속 시간
const float SLOW_ENEMY_DURATION = 3.0f;

//속도 저하 배율
const float SLOW_ENEMY_RATIO = 0.5f;

//범위 공격 반경
const float AREA_ATTACK_RADIUS = 400.0f;

//범위 공격 발동 간격
const float AREA_ATTACK_INTERVAL = 6.0f;

//범위 공격 데미지 비율
const float AREA_ATTACK_DAMAGE_RATIO = 1.0f;

//지속 공격 반경
const float CONTINUOUS_ATTACK_RADIUS = 300.0f;

//지속 공격 발동 간격
const float CONTINUOUS_ATTACK_INTERVAL = 1.0f;

//지속 공격 데미지 비율
const float CONTINUOUS_ATTACK_DAMAGE_RATIO = 0.3f;
