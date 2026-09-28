#include "MainMenuGameMode.h"

#include "MainPlayerController.h"

AMainMenuGameMode::AMainMenuGameMode()
{
	//배경음 조절 키(위아래 M)가 이 컨트롤러에 묶여 있어서 메뉴에서도 조절하려면 이걸 써야 함
	//이 컨트롤러는 메인 메뉴일 때 입력 모드를 Game And UI로 두므로 마우스 클릭도 그대로 됨
	PlayerControllerClass = AMainPlayerController::StaticClass();

	//폰을 만들지 않음 메뉴 화면이라 조종할 것이 없고 빈 레벨에 캐릭터가 생기면 아래로 떨어지기만 함
	DefaultPawnClass = nullptr;
}
