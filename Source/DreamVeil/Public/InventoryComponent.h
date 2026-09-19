#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponTypes.h"
#include "InventoryComponent.generated.h"

class UWeaponBase;

//강화를 시도한 결과 UI가 결과 문구를 고를 때 씀
UENUM(BlueprintType)
enum class EPartEnhanceResult : uint8
{
	//한 단계 올라감
	Success,
	//꿈의 조각만 쓰고 그대로
	Fail,
	//실패하면서 파츠가 부서져 사라짐
	Destroyed,
	//꿈의 조각이 모자라서 시도하지 못함
	NotEnoughShards,
	//이미 최대 단계거나 없는 파츠라 시도하지 못함
	CannotEnhance
};

//파츠 목록이 바뀌었을 때 얻기 팔기 장착 해제 강화 파괴 전부 여기로 알림 UI는 받으면 목록을 다시 그릴 것
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

//꿈의 조각 개수가 바뀌었을 때
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnDreamShardsChanged,
	int32, DreamShards
);

//파츠를 새로 얻었을 때 몬스터 드롭이나 구매 획득 알림 UI용
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnPartAcquired,
	const FWeaponPart&, Part
);

//플레이어 인벤토리 무기 파츠와 재화인 꿈의 조각을 들고 있음
//상점 구매 판매 강화 장착은 전부 여기서 하고 끼운 파츠는 무기에게 넘겨서 수치에 반영함
//레벨을 넘길 때 내용은 GameInstance가 저장했다가 새 레벨의 플레이어에게 복원함
//플레이어에 붙이는 이유 파츠를 쓰는 건 플레이어뿐이고 플레이어 클래스가 이미 커서 보관과 강화는 따로 뗌
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DREAMVEIL_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	// 이벤트 UI가 받을 것

	//파츠 목록이 바뀜
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChanged OnInventoryChanged;

	//꿈의 조각이 바뀜
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnDreamShardsChanged OnDreamShardsChanged;

	//파츠를 새로 얻음
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnPartAcquired OnPartAcquired;

	// 조회

	//가진 파츠 전부 끼운 것과 안 끼운 것은 bEquipped로 구분 UI는 이 순서의 번호로 파츠를 가리킴
	UFUNCTION(BlueprintPure, Category = "Inventory")
	TArray<FWeaponPart> GetParts() const;

	//가진 꿈의 조각
	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetDreamShards() const;

	// 얻기

	//파츠를 인벤토리에 넣음 몬스터 드롭과 상점 구매가 부름 넣을 때는 항상 빠진 상태
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void AddPart(const FWeaponPart& Part);

	//꿈의 조각을 더함 0 이하면 무시
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void AddDreamShards(int32 Amount);

	//몬스터를 잡았을 때 꿈의 조각과 확률로 파츠를 받음 데미지 라이브러리가 몬스터가 죽는 순간 부름
	void ReceiveKillRewards(AActor* KilledActor);

	// 장착 컴퓨터의 소켓 UI

	//파츠를 무기에 끼움 같은 총 같은 칸에 있던 파츠는 빠져서 인벤토리에 남음 그 총에 없는 칸이면 실패
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool EquipPart(int32 PartIndex);

	//끼운 파츠를 뺌
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool UnequipPart(int32 PartIndex);

	// 강화

	//꿈의 조각을 내고 한 단계 강화를 시도 끼운 파츠도 강화할 수 있음 실패하면 낮은 확률로 파츠가 부서짐
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	EPartEnhanceResult EnhancePart(int32 PartIndex);

	//다음 단계 강화에 드는 꿈의 조각
	UFUNCTION(BlueprintPure, Category = "Inventory")
	static int32 GetEnhanceCost(const FWeaponPart& Part);

	//다음 단계 강화 성공 확률 0~1 최대 단계면 0
	UFUNCTION(BlueprintPure, Category = "Inventory")
	static float GetEnhanceSuccessChance(const FWeaponPart& Part);

	//다음 단계 강화에 실패했을 때 부서질 확률 0~1 최대 단계면 0
	UFUNCTION(BlueprintPure, Category = "Inventory")
	static float GetEnhanceDestroyChance(const FWeaponPart& Part);

	// 상점 컴퓨터의 상점 UI

	//꿈의 조각으로 파츠를 삼 해금 안 된 등급 보스 등급 그 총에 없는 칸 꿈의 조각 부족이면 실패
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool BuyPart(EWeaponSlot Weapon, EWeaponPartSlot Slot, EWeaponPartTier Tier);

	//안 쓰는 파츠를 팔아 꿈의 조각을 받음 끼운 파츠는 실수로 팔지 않게 먼저 빼야 팔 수 있음
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool SellPart(int32 PartIndex);

	//상점에서 이 등급을 팔고 있는지 그 레벨이 열려 있어야 팔고 보스 등급은 팔지 않음
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsTierForSale(EWeaponPartTier Tier) const;

	//등급별 구매 가격
	UFUNCTION(BlueprintPure, Category = "Inventory")
	static int32 GetBuyPrice(EWeaponPartTier Tier);

	//되팔 때 받는 꿈의 조각 강화할수록 비싸게 팔림
	UFUNCTION(BlueprintPure, Category = "Inventory")
	static int32 GetSellPrice(const FWeaponPart& Part);

	// 파츠 수치 UI 표시용

	//이 파츠가 올려주는 공격력
	UFUNCTION(BlueprintPure, Category = "Inventory")
	static float GetPartDamageBonus(const FWeaponPart& Part);

	//이 파츠가 줄여주는 발사 간격 비율
	UFUNCTION(BlueprintPure, Category = "Inventory")
	static float GetPartFireRateBonus(const FWeaponPart& Part);

	// 저장과 복원 GameInstance가 씀

	//저장해둔 내용으로 되돌림 플레이어 BeginPlay에서 GameInstance가 부름
	void RestoreInventory(const TArray<FWeaponPart>& SavedParts, int32 SavedDreamShards);

private:
	//가진 파츠 전부 PIE 중에 Details에서 내용을 확인할 수 있게 보이게만 함
	UPROPERTY(VisibleInstanceOnly, Category = "Inventory")
	TArray<FWeaponPart> Parts;

	//가진 꿈의 조각
	UPROPERTY(VisibleInstanceOnly, Category = "Inventory")
	int32 DreamShards = 0;

	//꿈의 조각을 씀 모자라면 쓰지 않고 false
	bool SpendDreamShards(int32 Amount);

	//끼운 파츠를 총마다 모아서 무기에게 넘김 장착 강화 파괴 복원 뒤에 부름
	void ApplyPartsToWeapons();

	//등급만 정하고 총과 칸은 랜덤인 파츠를 만듦 몬스터 드롭용
	FWeaponPart MakeRandomPart(EWeaponPartTier Tier) const;

	//지금 레벨에 맞는 드롭 등급 L2면 Level2 레벨 맵이 아니면 Level1
	EWeaponPartTier GetCurrentLevelTier() const;

	//인벤토리 주인인 플레이어의 무기
	UWeaponBase* FindWeapon(EWeaponSlot Weapon) const;
};
