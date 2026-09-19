#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "AugmentTypes.h"
#include "WeaponTypes.h"
#include "DreamVeilGameInstance.generated.h"

class UDispatchTableComponent;
class UInventoryComponent;
class APawn;

//난이도 죽거나 시간 초과로 실패했을 때 파츠와 꿈의 조각을 얼마나 잃는지
//팀에서 상의해서 안 쓸 난이도는 빼도 됨 규칙은 ApplyDeathPenalty와 FailCurrentLevel에만 있음
UENUM(BlueprintType)
enum class EGameDifficulty : uint8
{
	//죽거나 실패해도 전부 유지 메인 메뉴에서 새 게임을 시작할 때만 초기화
	Easy,
	//죽거나 실패하면 그 레벨에서 얻은 것만 잃고 레벨에 들어가기 전으로 돌아감
	Normal,
	//죽으면 전부 초기화 시간 초과는 보통과 같음
	Hard
};

//레벨이 바뀌어도 남는 게임 전체 정보
//플레이어가 얻은 증강 기록을 레벨을 넘기기 전에 저장하고 새 레벨의 플레이어에게 복원함
//레벨을 다시 로드해도 사라지지 않으므로 게임 오버 뒤 재시작할 때는 ClearPlayerAugments로 비울 것
//깬 레벨 수도 여기서 들고 있어서 로비가 다음에 열 레벨과 Endless 개방 여부를 알 수 있음
//파츠와 꿈의 조각이 든 인벤토리도 증강과 같은 방식으로 저장하고 복원함 실패했을 때 얼마나 잃을지는 난이도가 정함
UCLASS()
class DREAMVEIL_API UDreamVeilGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	//플레이어의 증강 기록을 저장 다음 레벨을 열기 직전에 부를 것
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void SavePlayerAugments(UDispatchTableComponent* PlayerDispatchTable);

	//저장한 증강 기록을 새 레벨의 플레이어에게 다시 적용 플레이어 BeginPlay에서 부를 것
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool RestorePlayerAugments(UDispatchTableComponent* PlayerDispatchTable);

	//저장한 증강 기록을 비움 게임 오버 뒤 레벨을 다시 로드하기 전에 부를 것 안 비우면 새 판에 이전 판 증강이 남음
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void ClearPlayerAugments();

	//메인 메뉴에서 새 게임 시작 진행도와 증강 기록을 비우고 로비로 이동
	UFUNCTION(BlueprintCallable, Category = "Level")
	void StartNewGame();

	//로비에서 게임 시작 아직 안 깬 다음 레벨로 이동 L4까지 다 깼으면 열 레벨이 없어서 false
	UFUNCTION(BlueprintCallable, Category = "Level")
	bool OpenNextLevel();

	//지금 레벨을 깼을 때 진행도를 올리고 로비로 돌아감 게임모드가 제한 시간 안에 다 잡았거나 보스를 잡았을 때 부름
	UFUNCTION(BlueprintCallable, Category = "Level")
	void CompleteCurrentLevel();

	//제한 시간 안에 못 깼을 때 진행도는 그대로 두고 로비로 돌아감 이번 판에 얻은 증강도 저장하지 않음
	UFUNCTION(BlueprintCallable, Category = "Level")
	void FailCurrentLevel();

	//L4까지 다 깨서 Endless가 열렸는지 로비 UI가 Endless 버튼을 켤지 정할 때 씀
	UFUNCTION(BlueprintCallable, Category = "Level")
	bool IsEndlessUnlocked() const;

	//지금 맵이 L1~L4 중 하나인지 로비나 메인 메뉴면 false 게임모드가 레벨 제한 시간을 걸지 정할 때 씀
	UFUNCTION(BlueprintPure, Category = "Level")
	bool IsInLevelMap() const;

	//지금 맵의 레벨 번호 L1이면 1 로비나 메인 메뉴면 0 몬스터 드롭 등급을 정할 때 씀
	UFUNCTION(BlueprintPure, Category = "Level")
	int32 GetCurrentLevelNumber() const;

	//그 레벨에 들어갈 수 있는지 L1은 처음부터 열려 있고 하나 깰 때마다 다음 레벨이 열림 상점 등급 해금에도 씀
	UFUNCTION(BlueprintPure, Category = "Level")
	bool IsLevelUnlocked(int32 LevelNumber) const;

	// 난이도

	//난이도를 정함 메인 메뉴에서 새 게임을 시작하기 전에 부를 것
	UFUNCTION(BlueprintCallable, Category = "Difficulty")
	void SetDifficulty(EGameDifficulty NewDifficulty);

	//지금 난이도
	UFUNCTION(BlueprintPure, Category = "Difficulty")
	EGameDifficulty GetDifficulty() const;

	// 인벤토리 저장

	//플레이어 인벤토리를 저장 다음 레벨을 열기 직전에 부름
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SavePlayerInventory(UInventoryComponent* PlayerInventory);

	//저장한 인벤토리를 새 레벨의 플레이어에게 복원 플레이어 BeginPlay에서 부름
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RestorePlayerInventory(UInventoryComponent* PlayerInventory);

	//저장한 인벤토리를 비움 새 게임을 시작하거나 어려움 난이도에서 죽었을 때
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ClearPlayerInventory();

	//플레이어가 죽었을 때 난이도에 맞춰 인벤토리 저장본을 정리 플레이어 HandleDead에서 부름
	void ApplyDeathPenalty(UInventoryComponent* PlayerInventory);

private:
	//저장한 플레이어 증강 번호 얻은 순서대로 같은 번호가 여러 번이면 그만큼 중첩
	UPROPERTY(Transient)
	TArray<EAugmentID> SavedPlayerAugmentHistory;

	//깬 레벨 수 0이면 아무것도 안 깬 상태 다음에 열 레벨의 인덱스로도 그대로 씀
	int32 ClearedLevelCount = 0;

	//난이도 기본은 보통
	EGameDifficulty Difficulty = EGameDifficulty::Normal;

	//저장한 파츠 목록 끼운 상태도 같이 들어 있음
	UPROPERTY(Transient)
	TArray<FWeaponPart> SavedParts;

	//저장한 꿈의 조각
	int32 SavedDreamShards = 0;

	//지금 조종 중인 플레이어의 증강 기록과 인벤토리를 저장 맵을 떠나기 직전에 부름
	void SaveCurrentPlayerProgress();

	//지금 조종 중인 플레이어 폰 없으면 nullptr
	APawn* FindCurrentPlayerPawn() const;
};
