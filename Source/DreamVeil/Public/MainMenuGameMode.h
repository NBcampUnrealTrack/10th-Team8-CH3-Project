#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MainMenuGameMode.generated.h"

//메인 메뉴 전용 게임모드 하는 일은 어떤 플레이어 컨트롤러를 쓸지 정하는 것뿐
//AMainGameModeBase를 쓰지 않는 이유 그쪽은 제한 시간 잠식도 몬스터 등록까지 맡고 있어서 메뉴 화면에 필요 없음
//엔진 기본 GameModeBase도 못 쓰는 이유 그러면 플레이어 컨트롤러가 엔진 기본이라 배경음 조절 키가 없음
UCLASS()
class DREAMVEIL_API AMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMainMenuGameMode();
};
