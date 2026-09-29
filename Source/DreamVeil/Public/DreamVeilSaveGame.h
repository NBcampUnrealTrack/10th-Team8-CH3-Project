#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
//난이도(EGameDifficulty)가 여기 있어서 같이 가져옴 증강 번호와 파츠 정의도 아래 두 헤더에서 옴
#include "DreamVeilGameInstance.h"
#include "AugmentTypes.h"
#include "WeaponTypes.h"
#include "DreamVeilSaveGame.generated.h"

//게임을 껐다 켜도 남는 저장 파일 한 판의 진행 상황을 통째로 담음
//GameInstance가 들고 있는 값을 그대로 옮겨 담기만 하는 그릇이라 함수가 없음
//저장 시점은 로비에 도착할 때마다 자동 슬롯은 하나만 씀
//새 항목을 저장하고 싶으면 여기에 UPROPERTY를 추가하고 GameInstance의 SaveGameToSlot LoadGameFromSlot 양쪽에 한 줄씩 더하면 됨
UCLASS()
class DREAMVEIL_API UDreamVeilSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	//깬 레벨 수 다음에 열 레벨을 정함
	UPROPERTY()
	int32 ClearedLevelCount = 0;

	//저장할 때의 난이도 불러오면 그 난이도로 이어서 함
	UPROPERTY()
	EGameDifficulty Difficulty = EGameDifficulty::Normal;

	//얻은 증강 번호 얻은 순서대로 같은 번호가 여러 번이면 그만큼 중첩
	UPROPERTY()
	TArray<EAugmentID> AugmentHistory;

	//가진 파츠 끼운 상태도 같이 들어 있음
	UPROPERTY()
	TArray<FWeaponPart> Parts;

	//가진 꿈의 조각
	UPROPERTY()
	int32 DreamShards = 0;

	//가진 무기 사실상 소총을 샀는지 기록
	UPROPERTY()
	TArray<EWeaponSlot> WeaponSlots;

	//플레이어 레벨 정예 몬스터 확률이 이 값을 보므로 같이 저장해야 이어할 때 난이도가 맞음
	UPROPERTY()
	int32 PlayerLevel = 1;

	//지금 레벨에서 모은 경험치
	UPROPERTY()
	float CurrentExperience = 0.0f;

	//저장한 시각 메인 메뉴의 이어하기 버튼에 언제 저장했는지 보여주려고 같이 담음
	UPROPERTY()
	FDateTime SaveTime;
};
