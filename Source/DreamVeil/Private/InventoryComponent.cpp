#include "InventoryComponent.h"

#include "DreamVeilGameInstance.h"
#include "EliteMonster.h"
#include "MainGameModeBase.h"
#include "MainPlayerCharacter.h"
#include "MonsterBase.h"
#include "WeaponBase.h"

//상점 강화 드롭 수치 여기 숫자만 바꾸면 밸런스 조절
//등급 순서 Level1 Level2 Level3 Level4 Boss

//구매 가격 보스 파츠는 상점에서 팔지 않지만 되팔 때 가격 계산에 씀
const int32 PART_PRICE_BY_TIER[] = { 30, 60, 100, 150, 250 };

//되팔 때 구매가의 몇 퍼센트를 돌려주는지 강화 단계와 상관없이 등급 가격으로만 정함
const float PART_SELL_RATIO = 0.3f;

//소총 구매 가격 임시값
const int32 RIFLE_PRICE = 200;

//소총을 팔기 시작하는 레벨 L2를 깨서 L3가 열리면 로비 상점에서 살 수 있음
const int32 RIFLE_UNLOCK_LEVEL = 3;

//강화 기본 비용 목표 단계를 곱해서 씀 Level1 파츠를 +3으로 올리려면 10 x 3 = 30
const int32 ENHANCE_COST_BY_TIER[] = { 10, 20, 35, 50, 80 };

//강화 성공 확률 목표 단계 순서 +1 +2 +3 +4 +5 올라갈수록 빡세짐
const float ENHANCE_SUCCESS_CHANCE[] = { 0.9f, 0.7f, 0.5f, 0.3f, 0.15f };

//강화에 실패했을 때 파츠가 부서질 확률 목표 단계 순서 +1부터 부서질 수 있음
//실패한 뒤에 한 번 더 굴리므로 한 번 시도할 때 실제로 부서질 확률은 (1 - 성공 확률) x 이 값
//+1 0.5% +2 2.4% +3 6% +4 12.6% +5 21.3%
const float ENHANCE_DESTROY_CHANCE[] = { 0.05f, 0.08f, 0.12f, 0.18f, 0.25f };

//몬스터가 파츠를 떨굴 확률 보스는 무조건 보스 파츠를 떨굼
const float NORMAL_PART_DROP_CHANCE = 0.02f;
const float ELITE_PART_DROP_CHANCE = 0.1f;

//몬스터가 떨구는 꿈의 조각 개수 범위
const int32 NORMAL_SHARD_MIN = 1;
const int32 NORMAL_SHARD_MAX = 3;
const int32 ELITE_SHARD_MIN = 5;
const int32 ELITE_SHARD_MAX = 10;
const int32 BOSS_SHARD_REWARD = 50;

//표의 칸 수가 등급 수나 최대 강화 단계와 어긋나면 컴파일 단계에서 바로 알 수 있게 막음
static_assert(static_cast<int32>(UE_ARRAY_COUNT(PART_PRICE_BY_TIER)) == static_cast<int32>(EWeaponPartTier::Boss) + 1, "PART_PRICE_BY_TIER needs one value per tier");
static_assert(static_cast<int32>(UE_ARRAY_COUNT(ENHANCE_COST_BY_TIER)) == static_cast<int32>(EWeaponPartTier::Boss) + 1, "ENHANCE_COST_BY_TIER needs one value per tier");
static_assert(static_cast<int32>(UE_ARRAY_COUNT(ENHANCE_SUCCESS_CHANCE)) == MAX_PART_ENHANCE_LEVEL, "ENHANCE_SUCCESS_CHANCE needs one value per enhance level");
static_assert(static_cast<int32>(UE_ARRAY_COUNT(ENHANCE_DESTROY_CHANCE)) == MAX_PART_ENHANCE_LEVEL, "ENHANCE_DESTROY_CHANCE needs one value per enhance level");

UInventoryComponent::UInventoryComponent()
{
	//시간에 따라 할 일이 없어서 Tick을 끔
	PrimaryComponentTick.bCanEverTick = false;
}

//가진 파츠 전부
TArray<FWeaponPart> UInventoryComponent::GetParts() const
{
	return Parts;
}

//가진 꿈의 조각
int32 UInventoryComponent::GetDreamShards() const
{
	return DreamShards;
}

//파츠를 인벤토리에 넣음
void UInventoryComponent::AddPart(const FWeaponPart& Part)
{
	//얻은 파츠는 항상 빠진 상태로 들어옴 끼우는 건 플레이어가 소켓 UI에서 고름
	FWeaponPart NewPart = Part;
	NewPart.bEquipped = false;

	Parts.Add(NewPart);

	//배열 안의 참조 대신 복사본을 넘김 알림을 받은 쪽이 그 자리에서 파츠를 또 넣어도 참조가 깨지지 않게
	OnPartAcquired.Broadcast(NewPart);
	OnInventoryChanged.Broadcast();
}

//꿈의 조각을 더함
void UInventoryComponent::AddDreamShards(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}

	DreamShards += Amount;

	OnDreamShardsChanged.Broadcast(DreamShards);
}

//꿈의 조각을 씀
bool UInventoryComponent::SpendDreamShards(int32 Amount)
{
	//모자라면 하나도 쓰지 않음 반만 빠지는 일이 없게
	if (Amount < 0 || DreamShards < Amount)
	{
		return false;
	}

	DreamShards -= Amount;

	OnDreamShardsChanged.Broadcast(DreamShards);

	return true;
}

//몬스터를 잡았을 때
void UInventoryComponent::ReceiveKillRewards(AActor* KilledActor)
{
	//몬스터가 아닌 것을 잡았을 때는 보상 없음
	if (!Cast<AMonsterBase>(KilledActor))
	{
		return;
	}

	//보스 판정은 게임모드와 같은 기준을 써서 레벨 클리어 조건과 드롭이 어긋나지 않게 함
	const bool bBoss = AMainGameModeBase::IsBossMonster(KilledActor);
	const bool bElite = KilledActor->IsA<AEliteMonster>();

	//꿈의 조각은 잡을 때마다 받음
	if (bBoss)
	{
		AddDreamShards(BOSS_SHARD_REWARD);
	}
	else if (bElite)
	{
		AddDreamShards(FMath::RandRange(ELITE_SHARD_MIN, ELITE_SHARD_MAX));
	}
	else
	{
		AddDreamShards(FMath::RandRange(NORMAL_SHARD_MIN, NORMAL_SHARD_MAX));
	}

	//보스는 보스 파츠를 무조건 떨굼 엔드리스를 대비해 L4를 계속 돌게 만드는 보상
	if (bBoss)
	{
		AddPart(MakeRandomPart(EWeaponPartTier::Boss));
		return;
	}

	//일반과 엘리트는 아주 낮은 확률로 지금 레벨 등급의 파츠를 떨굼
	const float PartDropChance = bElite ? ELITE_PART_DROP_CHANCE : NORMAL_PART_DROP_CHANCE;

	if (FMath::FRand() < PartDropChance)
	{
		AddPart(MakeRandomPart(GetCurrentLevelTier()));
	}
}

//파츠를 무기에 끼움
bool UInventoryComponent::EquipPart(int32 PartIndex)
{
	if (!Parts.IsValidIndex(PartIndex))
	{
		return false;
	}

	FWeaponPart& PartToEquip = Parts[PartIndex];
	UWeaponBase* Weapon = FindWeapon(PartToEquip.Weapon);

	//그 총에 없는 칸이면 못 낌 권총에 개머리판을 끼우려는 경우
	if (!Weapon || !Weapon->GetPartSlots().Contains(PartToEquip.Slot))
	{
		return false;
	}

	//같은 총 같은 칸에 끼워져 있던 파츠는 빼서 인벤토리에 남겨둠 버리지 않음
	for (FWeaponPart& Part : Parts)
	{
		if (Part.bEquipped && Part.Weapon == PartToEquip.Weapon && Part.Slot == PartToEquip.Slot)
		{
			Part.bEquipped = false;
		}
	}

	PartToEquip.bEquipped = true;

	ApplyPartsToWeapons();
	OnInventoryChanged.Broadcast();

	return true;
}

//끼운 파츠를 뺌
bool UInventoryComponent::UnequipPart(int32 PartIndex)
{
	if (!Parts.IsValidIndex(PartIndex) || !Parts[PartIndex].bEquipped)
	{
		return false;
	}

	Parts[PartIndex].bEquipped = false;

	ApplyPartsToWeapons();
	OnInventoryChanged.Broadcast();

	return true;
}

//한 단계 강화를 시도
EPartEnhanceResult UInventoryComponent::EnhancePart(int32 PartIndex)
{
	if (!Parts.IsValidIndex(PartIndex) || Parts[PartIndex].EnhanceLevel >= MAX_PART_ENHANCE_LEVEL)
	{
		return EPartEnhanceResult::CannotEnhance;
	}

	//성공 실패와 상관없이 시도할 때 비용을 냄
	if (!SpendDreamShards(GetEnhanceCost(Parts[PartIndex])))
	{
		return EPartEnhanceResult::NotEnoughShards;
	}

	EPartEnhanceResult Result = EPartEnhanceResult::Fail;

	if (FMath::FRand() < GetEnhanceSuccessChance(Parts[PartIndex]))
	{
		Parts[PartIndex].EnhanceLevel++;
		Result = EPartEnhanceResult::Success;
	}
	else if (FMath::FRand() < GetEnhanceDestroyChance(Parts[PartIndex]))
	{
		//실패한 뒤 한 번 더 굴려서 낮은 확률로 파츠가 부서짐 끼워져 있던 파츠면 무기에서도 빠짐
		Parts.RemoveAt(PartIndex);
		Result = EPartEnhanceResult::Destroyed;
	}

	//끼워져 있던 파츠면 수치가 바뀌었거나 사라졌으니 무기에 다시 반영
	ApplyPartsToWeapons();
	OnInventoryChanged.Broadcast();

	return Result;
}

//다음 단계 강화 비용
int32 UInventoryComponent::GetEnhanceCost(const FWeaponPart& Part)
{
	//목표 단계를 곱해서 높은 단계로 갈수록 비싸짐
	return ENHANCE_COST_BY_TIER[static_cast<int32>(Part.Tier)] * (Part.EnhanceLevel + 1);
}

//다음 단계 강화 성공 확률
float UInventoryComponent::GetEnhanceSuccessChance(const FWeaponPart& Part)
{
	if (Part.EnhanceLevel < 0 || Part.EnhanceLevel >= MAX_PART_ENHANCE_LEVEL)
	{
		return 0.0f;
	}

	//지금 단계가 곧 다음 단계 확률 표의 번호 0강이면 0번인 +1 확률
	return ENHANCE_SUCCESS_CHANCE[Part.EnhanceLevel];
}

//다음 단계 강화 실패 시 파괴 확률
float UInventoryComponent::GetEnhanceDestroyChance(const FWeaponPart& Part)
{
	if (Part.EnhanceLevel < 0 || Part.EnhanceLevel >= MAX_PART_ENHANCE_LEVEL)
	{
		return 0.0f;
	}

	return ENHANCE_DESTROY_CHANCE[Part.EnhanceLevel];
}

//상점에서 파츠를 삼
bool UInventoryComponent::BuyPart(EWeaponSlot Weapon, EWeaponPartSlot Slot, EWeaponPartTier Tier)
{
	if (!IsTierForSale(Tier))
	{
		return false;
	}

	//그 총에 없는 칸은 팔지 않음 꿈의 조각을 쓰기 전에 먼저 걸러야 헛돈을 안 씀
	UWeaponBase* TargetWeapon = FindWeapon(Weapon);

	if (!TargetWeapon || !TargetWeapon->GetPartSlots().Contains(Slot))
	{
		return false;
	}

	if (!SpendDreamShards(GetBuyPrice(Tier)))
	{
		return false;
	}

	FWeaponPart NewPart;
	NewPart.Weapon = Weapon;
	NewPart.Slot = Slot;
	NewPart.Tier = Tier;

	AddPart(NewPart);

	return true;
}

//안 쓰는 파츠를 팖
bool UInventoryComponent::SellPart(int32 PartIndex)
{
	//끼운 파츠는 실수로 팔지 않게 막음 소켓 UI에서 먼저 빼고 팔 것
	if (!Parts.IsValidIndex(PartIndex) || Parts[PartIndex].bEquipped)
	{
		return false;
	}

	//지우기 전에 값을 계산해둠 지운 뒤에는 그 번호에 다른 파츠가 들어옴
	const int32 SellPrice = GetSellPrice(Parts[PartIndex]);

	Parts.RemoveAt(PartIndex);

	AddDreamShards(SellPrice);
	OnInventoryChanged.Broadcast();

	return true;
}

//상점에서 이 등급을 팔고 있는지
bool UInventoryComponent::IsTierForSale(EWeaponPartTier Tier) const
{
	//보스 파츠는 보스를 잡아야만 얻음 상점에서 팔면 L4를 계속 돌 이유가 사라짐
	if (Tier == EWeaponPartTier::Boss)
	{
		return false;
	}

	UDreamVeilGameInstance* DreamVeilGameInstance = GetOwner() ? GetOwner()->GetGameInstance<UDreamVeilGameInstance>() : nullptr;

	if (!DreamVeilGameInstance)
	{
		return false;
	}

	//Level1이 0번이라 1을 더하면 레벨 번호 그 레벨이 열려 있어야 그 등급을 팖
	return DreamVeilGameInstance->IsLevelUnlocked(static_cast<int32>(Tier) + 1);
}

//등급별 구매 가격
int32 UInventoryComponent::GetBuyPrice(EWeaponPartTier Tier)
{
	return PART_PRICE_BY_TIER[static_cast<int32>(Tier)];
}

//되팔 때 받는 꿈의 조각
int32 UInventoryComponent::GetSellPrice(const FWeaponPart& Part)
{
	//강화해도 판매가는 그대로 강화에 쓴 꿈의 조각은 돌려받지 못함
	return FMath::RoundToInt(PART_PRICE_BY_TIER[static_cast<int32>(Part.Tier)] * PART_SELL_RATIO);
}

//무기를 삼
bool UInventoryComponent::BuyWeapon(EWeaponSlot Weapon)
{
	//이미 가졌거나 아직 해금 전이거나 팔지 않는 무기면 꿈의 조각을 쓰기 전에 거름
	if (!IsWeaponForSale(Weapon))
	{
		return false;
	}

	AMainPlayerCharacter* OwnerPlayer = Cast<AMainPlayerCharacter>(GetOwner());

	if (!OwnerPlayer || !SpendDreamShards(GetWeaponPrice(Weapon)))
	{
		return false;
	}

	//얻기만 하고 바로 들지는 않음 숫자 2로 바꿔 듦
	OwnerPlayer->AcquireWeapon(Weapon);

	//이제 이 총의 파츠도 사고 끼울 수 있어서 소켓 상점 UI가 목록을 다시 그려야 함
	OnInventoryChanged.Broadcast();

	return true;
}

//상점에서 이 무기를 팔고 있는지
bool UInventoryComponent::IsWeaponForSale(EWeaponSlot Weapon) const
{
	//지금 파는 무기는 소총뿐 권총은 처음부터 가지고 있음
	//이미 가진 무기면 FindWeapon이 무기를 찾아서 다시 팔지 않음
	if (Weapon != EWeaponSlot::Rifle || FindWeapon(Weapon))
	{
		return false;
	}

	UDreamVeilGameInstance* DreamVeilGameInstance = GetOwner() ? GetOwner()->GetGameInstance<UDreamVeilGameInstance>() : nullptr;

	return DreamVeilGameInstance && DreamVeilGameInstance->IsLevelUnlocked(RIFLE_UNLOCK_LEVEL);
}

//무기 구매 가격 팔지 않는 무기는 0
int32 UInventoryComponent::GetWeaponPrice(EWeaponSlot Weapon)
{
	return Weapon == EWeaponSlot::Rifle ? RIFLE_PRICE : 0;
}

//이 파츠가 올려주는 공격력
float UInventoryComponent::GetPartDamageBonus(const FWeaponPart& Part)
{
	return CalculatePartDamageBonus(Part);
}

//이 파츠가 줄여주는 발사 간격 비율
float UInventoryComponent::GetPartFireRateBonus(const FWeaponPart& Part)
{
	return CalculatePartFireRateBonus(Part);
}

//저장해둔 내용으로 되돌림
void UInventoryComponent::RestoreInventory(const TArray<FWeaponPart>& SavedParts, int32 SavedDreamShards)
{
	Parts = SavedParts;
	DreamShards = FMath::Max(SavedDreamShards, 0);

	//끼워져 있던 파츠를 새 레벨의 무기에도 다시 반영 무기는 레벨이 바뀔 때 새로 만들어져서 파츠를 모름
	ApplyPartsToWeapons();

	OnInventoryChanged.Broadcast();
	OnDreamShardsChanged.Broadcast(DreamShards);
}

//끼운 파츠를 총마다 모아서 무기에게 넘김
void UInventoryComponent::ApplyPartsToWeapons()
{
	//총이 늘어나면 여기에 추가
	for (EWeaponSlot WeaponSlot : { EWeaponSlot::Pistol, EWeaponSlot::Rifle })
	{
		UWeaponBase* Weapon = FindWeapon(WeaponSlot);

		if (!Weapon)
		{
			continue;
		}

		TArray<FWeaponPart> EquippedParts;

		for (const FWeaponPart& Part : Parts)
		{
			if (Part.bEquipped && Part.Weapon == WeaponSlot)
			{
				EquippedParts.Add(Part);
			}
		}

		Weapon->SetEquippedParts(EquippedParts);
	}
}

//등급만 정하고 총과 칸은 랜덤인 파츠를 만듦
FWeaponPart UInventoryComponent::MakeRandomPart(EWeaponPartTier Tier) const
{
	FWeaponPart NewPart;
	NewPart.Tier = Tier;

	//가진 총의 파츠만 나옴 소총을 사기 전에는 권총 파츠만 나오고 산 뒤로는 반반
	NewPart.Weapon = (FindWeapon(EWeaponSlot::Rifle) && FMath::RandBool()) ? EWeaponSlot::Rifle : EWeaponSlot::Pistol;

	//칸은 그 총이 가진 칸 중에서만 고름 권총 파츠가 개머리판으로 나오지 않게
	//무기를 못 찾으면 기본값인 총구로 둠 총구는 모든 총이 가진 공용 칸이라 끼울 수 있음
	UWeaponBase* Weapon = FindWeapon(NewPart.Weapon);

	if (Weapon)
	{
		const TArray<EWeaponPartSlot> Slots = Weapon->GetPartSlots();

		if (Slots.Num() > 0)
		{
			NewPart.Slot = Slots[FMath::RandRange(0, Slots.Num() - 1)];
		}
	}

	return NewPart;
}

//지금 레벨에 맞는 드롭 등급
EWeaponPartTier UInventoryComponent::GetCurrentLevelTier() const
{
	UDreamVeilGameInstance* DreamVeilGameInstance = GetOwner() ? GetOwner()->GetGameInstance<UDreamVeilGameInstance>() : nullptr;
	const int32 LevelNumber = DreamVeilGameInstance ? DreamVeilGameInstance->GetCurrentLevelNumber() : 0;

	//레벨 번호에서 1을 빼면 등급 번호 레벨 맵이 아니면(테스트 맵 등) 0이 나와서 Level1
	const int32 TierIndex = FMath::Clamp(LevelNumber - 1, 0, static_cast<int32>(EWeaponPartTier::Level4));

	return static_cast<EWeaponPartTier>(TierIndex);
}

//인벤토리 주인인 플레이어가 가진 무기
UWeaponBase* UInventoryComponent::FindWeapon(EWeaponSlot Weapon) const
{
	AMainPlayerCharacter* OwnerPlayer = Cast<AMainPlayerCharacter>(GetOwner());

	//소총 컴포넌트는 사기 전에도 숨겨진 채로 있어서 가졌는지 따로 확인함
	//여기서 한 번 거르면 파츠 드롭 구매 장착이 전부 가진 총 기준으로 맞춰짐
	if (!OwnerPlayer || !OwnerPlayer->HasWeapon(Weapon))
	{
		return nullptr;
	}

	return OwnerPlayer->GetWeaponInSlot(Weapon);
}
