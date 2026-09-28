#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponTypes.h"
#include "InventoryComponent.generated.h"

class UWeaponBase;

//상점에서 한 동작(구매 판매 강화)의 결과 UI가 결과 문구를 고를 때 씀
//셋이 같은 enum을 쓰는 이유 세 화면 모두 결과를 같은 자리에 한 줄로 띄우는데
//동작마다 enum을 따로 두면 문구를 만드는 함수도 세 개가 돼서 같은 말을 세 군데에서 고치게 됨
//성공도 Bought Sold Enhanced로 나눠둔 이유 값 하나에 문구 하나가 붙어야 문구표에 갈래가 안 생김
UENUM(BlueprintType)
enum class EShopResult : uint8
{
	//구매 성공 파츠와 무기가 같이 씀
	Bought,
	//판매 성공
	Sold,
	//강화 성공 한 단계 올라감
	Enhanced,
	//강화 실패 꿈의 조각만 쓰고 그대로
	EnhanceFailed,
	//강화에 실패하면서 파츠가 부서져 사라짐
	PartDestroyed,
	//꿈의 조각이 모자라서 시도하지 못함 구매와 강화가 같이 씀
	NotEnoughShards,
	//아직 안 열린 등급이거나 그 총에 없는 칸이라 살 수 없음
	NotForSale,
	//끼운 파츠라 팔 수 없음
	CannotSell,
	//이미 최대 단계거나 없는 파츠라 더 강화할 수 없음
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

//구매 판매 강화를 시도할 때마다 그 결과를 알림 UI가 결과 문구를 띄울 때 씀
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnShopResult,
	EShopResult, Result
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

	//구매 판매 강화 결과가 나옴
	//함수가 결과를 돌려주는데도 알림을 따로 보내는 이유
	//구매 버튼만 서른 개가 넘어서 버튼마다 문구를 이어 붙이면 위젯에서 같은 연결을 서른 번 해야 함
	//이걸 한 번만 묶어두면 세 화면에서 무엇을 하든 결과가 문구 한 칸으로 들어옴
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnShopResult OnShopResult;

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
	EShopResult EnhancePart(int32 PartIndex);

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
	//bool이 아니라 결과를 돌려주는 이유 UI가 못 산 이유까지 알아야 꿈의 조각 부족을 따로 안내할 수 있음
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	EShopResult BuyPart(EWeaponSlot Weapon, EWeaponPartSlot Slot, EWeaponPartTier Tier);

	//안 쓰는 파츠를 팔아 꿈의 조각을 받음 끼운 파츠는 실수로 팔지 않게 먼저 빼야 팔 수 있음
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	EShopResult SellPart(int32 PartIndex);

	//상점에서 이 등급을 팔고 있는지 그 레벨이 열려 있어야 팔고 보스 등급은 팔지 않음
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsTierForSale(EWeaponPartTier Tier) const;

	//등급별 구매 가격
	UFUNCTION(BlueprintPure, Category = "Inventory")
	static int32 GetBuyPrice(EWeaponPartTier Tier);

	//되팔 때 받는 꿈의 조각 등급 가격의 일정 비율 강화 단계와 상관없음
	UFUNCTION(BlueprintPure, Category = "Inventory")
	static int32 GetSellPrice(const FWeaponPart& Part);

	//꿈의 조각으로 무기를 삼 지금은 소총만 팔고 L3가 열려야 살 수 있음 이미 가진 무기면 실패
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	EShopResult BuyWeapon(EWeaponSlot Weapon);

	//상점에서 이 무기를 팔고 있는지 UI가 무기 구매 버튼을 켤지 정할 때 씀
	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsWeaponForSale(EWeaponSlot Weapon) const;

	//무기 구매 가격 팔지 않는 무기는 0
	UFUNCTION(BlueprintPure, Category = "Inventory")
	static int32 GetWeaponPrice(EWeaponSlot Weapon);

	// 파츠 수치 UI 표시용

	//이 파츠가 올려주는 공격력
	UFUNCTION(BlueprintPure, Category = "Inventory")
	static float GetPartDamageBonus(const FWeaponPart& Part);

	//이 파츠가 줄여주는 발사 간격 비율
	UFUNCTION(BlueprintPure, Category = "Inventory")
	static float GetPartFireRateBonus(const FWeaponPart& Part);

	//결과를 화면에 띄울 한 줄 문구로 바꿈 구매 판매 강화 결과가 전부 이걸 거쳐서 같은 말투로 뜸
	//블루프린트에서 결과마다 스위치를 짜지 않고 여기 둔 이유
	//문구를 고칠 때 위젯 세 화면을 돌아다니지 않고 이 함수 하나만 보면 되게 하려는 것
	UFUNCTION(BlueprintPure, Category = "Inventory")
	static FText GetShopResultText(EShopResult Result);

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

	//결과를 알리고 받은 결과를 그대로 돌려줌 return 자리에서 바로 쓰려고 돌려주는 형태로 둠
	//구매 판매 강화가 끝나는 자리마다 알림 한 줄을 붙여 넣으면 한 군데만 빠뜨려도 문구가 안 떠서 한곳으로 모음
	EShopResult NotifyShopResult(EShopResult Result);

	//끼운 파츠를 총마다 모아서 무기에게 넘김 장착 강화 파괴 복원 뒤에 부름
	void ApplyPartsToWeapons();

	//등급만 정하고 총과 칸은 랜덤인 파츠를 만듦 몬스터 드롭용 가진 총의 파츠만 나옴
	FWeaponPart MakeRandomPart(EWeaponPartTier Tier) const;

	//지금 레벨에 맞는 드롭 등급 L2면 Level2 레벨 맵이 아니면 Level1
	EWeaponPartTier GetCurrentLevelTier() const;

	//인벤토리 주인인 플레이어가 가진 무기 아직 얻지 못한 무기(사기 전 소총)면 nullptr
	UWeaponBase* FindWeapon(EWeaponSlot Weapon) const;
};
