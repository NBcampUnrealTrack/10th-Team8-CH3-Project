#pragma once

#include "CoreMinimal.h"
#include "WeaponTypes.generated.h"

//무기 종류 1번 권총 2번 소총
//원래 WeaponBase.h에 있었는데 파츠도 어느 총 것인지 알아야 해서 파츠 정의와 같이 여기로 옮김
UENUM(BlueprintType)
enum class EWeaponSlot : uint8
{
	Pistol,
	Rifle,
	//아무 무기도 들지 않은 맨손 상태 로비처럼 싸우지 않는 곳에서 씀
	//싸울 수 있는지를 bool로 따로 두지 않고 들고 있는 무기 칸 자체를 이 값으로 두는 이유
	//무기 숨기기 사격 막기 무기 교체 막기 맨손 애니메이션이 전부 이 값 하나로 정해져서 상태가 둘로 갈라지지 않음
	//애님 블루프린트의 Blend Poses (EWeaponSlot) 노드에 이 값의 핀을 만들지 않으면 Default Pose(맨손 Idle)가 재생됨
	//맨 뒤에 둔 이유 앞에 끼우면 Pistol Rifle의 번호가 밀려서 이미 저장된 에셋의 값이 어긋남
	Nothing
};

//파츠를 끼우는 칸
//앞의 세 칸은 모든 총이 같이 쓰는 공용 칸이고 뒤의 두 칸은 소총 전용
//어떤 총이 어떤 칸을 가졌는지는 UWeaponBase::GetPartSlots가 정함
UENUM(BlueprintType)
enum class EWeaponPartSlot : uint8
{
	//총구 공격력
	Muzzle,
	//탄창 연사력
	Magazine,
	//조준기 공격력
	Sight,
	//개머리판 소총 전용 연사력
	Stock,
	//앞손잡이 소총 전용 연사력
	Foregrip
};

//파츠 등급 몬스터는 자기 레벨 등급을 떨구고 보스 파츠는 보스만 떨굼
UENUM(BlueprintType)
enum class EWeaponPartTier : uint8
{
	Level1,
	Level2,
	Level3,
	Level4,
	Boss
};

//파츠 하나 인벤토리가 들고 있고 끼운 파츠는 무기에게도 복사해서 넘김
USTRUCT(BlueprintType)
struct FWeaponPart
{
	GENERATED_BODY()

	//어느 총의 파츠인지 권총 파츠는 권총에만 소총 파츠는 소총에만 낌
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	EWeaponSlot Weapon = EWeaponSlot::Pistol;

	//끼우는 칸
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	EWeaponPartSlot Slot = EWeaponPartSlot::Muzzle;

	//등급 높을수록 효과가 큼
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	EWeaponPartTier Tier = EWeaponPartTier::Level1;

	//강화 단계 0부터 MAX_PART_ENHANCE_LEVEL까지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	int32 EnhanceLevel = 0;

	//무기에 끼워져 있는지 같은 총 같은 칸에는 하나만 끼워짐
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Part")
	bool bEquipped = false;
};

//파츠 효과 수치 여기 숫자만 바꾸면 밸런스 조절
//등급 순서 Level1 Level2 Level3 Level4 Boss

//공격력 칸(총구 조준기)이 무기 데미지에 더해주는 값
const float PART_DAMAGE_BY_TIER[] = { 1.0f, 2.0f, 3.0f, 5.0f, 8.0f };

//연사력 칸(탄창 개머리판 앞손잡이)이 발사 간격을 줄여주는 비율 0.05면 5% 짧아짐
const float PART_FIRE_RATE_BY_TIER[] = { 0.03f, 0.05f, 0.08f, 0.12f, 0.18f };

//등급 수와 표의 칸 수가 어긋나면 컴파일 단계에서 바로 알 수 있게 막음
static_assert(static_cast<int32>(UE_ARRAY_COUNT(PART_DAMAGE_BY_TIER)) == static_cast<int32>(EWeaponPartTier::Boss) + 1, "PART_DAMAGE_BY_TIER needs one value per tier");
static_assert(static_cast<int32>(UE_ARRAY_COUNT(PART_FIRE_RATE_BY_TIER)) == static_cast<int32>(EWeaponPartTier::Boss) + 1, "PART_FIRE_RATE_BY_TIER needs one value per tier");

//강화 한 단계마다 기본 효과에 더해지는 비율 +5 강화면 기본 효과의 2배
const float PART_ENHANCE_BONUS_PER_LEVEL = 0.2f;

//최대 강화 단계
const int32 MAX_PART_ENHANCE_LEVEL = 5;

//연사력 파츠를 전부 합쳐도 발사 간격을 이 비율보다 더 줄이지 않음 너무 빨라지면 소총이 다시 기관총이 됨
const float MAX_PART_FIRE_RATE_BONUS = 0.6f;

//공격력 칸인지 아니면 연사력 칸
inline bool IsDamagePartSlot(EWeaponPartSlot Slot)
{
	return Slot == EWeaponPartSlot::Muzzle || Slot == EWeaponPartSlot::Sight;
}

//강화 단계를 반영한 배율 강화 0이면 1배
inline float GetPartEnhanceMultiplier(const FWeaponPart& Part)
{
	return 1.0f + PART_ENHANCE_BONUS_PER_LEVEL * Part.EnhanceLevel;
}

//이 파츠가 올려주는 데미지 연사력 칸이면 0
//무기와 인벤토리 UI가 같은 계산을 쓰도록 한 곳에 둠
inline float CalculatePartDamageBonus(const FWeaponPart& Part)
{
	if (!IsDamagePartSlot(Part.Slot))
	{
		return 0.0f;
	}

	return PART_DAMAGE_BY_TIER[static_cast<int32>(Part.Tier)] * GetPartEnhanceMultiplier(Part);
}

//이 파츠가 줄여주는 발사 간격 비율 공격력 칸이면 0
inline float CalculatePartFireRateBonus(const FWeaponPart& Part)
{
	if (IsDamagePartSlot(Part.Slot))
	{
		return 0.0f;
	}

	return PART_FIRE_RATE_BY_TIER[static_cast<int32>(Part.Tier)] * GetPartEnhanceMultiplier(Part);
}
