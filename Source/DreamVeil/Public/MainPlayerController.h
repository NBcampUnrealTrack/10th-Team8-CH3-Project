#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "MainPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;

UCLASS()
class DREAMVEIL_API AMainPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	AMainPlayerController();
	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputMappingContext* InputMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* SprintAction;

	//사격 입력 누르고 있으면 무기 연사 간격마다 발사
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* FireAction;

	//숫자 1 권총으로 바꾸기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* EquipPistolAction;

	//숫자 2 소총으로 바꾸기 소총을 얻기 전에는 눌러도 안 바뀜
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* EquipRifleAction;

	// 침대와 컴퓨터 상호작용 입력
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	UInputAction* InteractAction;

	//게임 오버 화면 위젯 블루프린트 BP_MainPlayerController의 Class Defaults에서 WBP_GameOver를 넣을 것
	//비워두면 죽어도 화면이 안 뜸
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UUserWidget> GameOverWidgetClass;

	//항상 떠 있는 화면 체력 스태미나 꿈의 조각 표시용 WBP_HUD를 넣을 것
	//레벨이 시작될 때 자동으로 뜨고 메뉴 위젯과 달리 입력 모드를 바꾸지 않음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	//메뉴 위젯을 띄우고 마우스로 누를 수 있게 함 침대 컴퓨터 게임 오버가 전부 이걸 씀
	//이미 열린 메뉴가 있으면 닫고 새로 엶 두 개가 겹쳐서 마우스가 먹통이 되는 걸 막음
	UFUNCTION(BlueprintCallable, Category = "UI")
	UUserWidget* OpenMenuWidget(TSubclassOf<UUserWidget> MenuWidgetClass);

	//열려 있는 메뉴 위젯을 닫고 게임 입력으로 되돌림 위젯의 닫기 버튼이 부를 것
	UFUNCTION(BlueprintCallable, Category = "UI")
	void CloseMenuWidget();

	virtual void BeginPlay() override;

protected:
	//조종할 폰이 정해질 때 불림 플레이어 캐릭터면 사망 이벤트를 구독함
	virtual void OnPossess(APawn* InPawn) override;

private:
	//플레이어가 죽었을 때 게임 오버 화면을 띄우고 마우스로 버튼을 누를 수 있게 함
	UFUNCTION()
	void ShowGameOver();

	//지금 열려 있는 메뉴 위젯 닫을 때 필요해서 들고 있음 없으면 nullptr
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> MenuWidgetInstance;
};
