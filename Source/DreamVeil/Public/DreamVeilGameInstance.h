#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "AugmentTypes.h"
#include "DreamVeilGameInstance.generated.h"

class UDispatchTableComponent;

//레벨이 바뀌어도 남는 게임 전체 정보
//플레이어가 얻은 증강 기록을 레벨을 넘기기 전에 저장하고 새 레벨의 플레이어에게 복원함
//레벨을 다시 로드해도 사라지지 않으므로 게임 오버 뒤 재시작할 때는 ClearPlayerAugments로 비울 것
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

private:
	//저장한 플레이어 증강 번호 얻은 순서대로 같은 번호가 여러 번이면 그만큼 중첩
	UPROPERTY(Transient)
	TArray<EAugmentID> SavedPlayerAugmentHistory;
};
