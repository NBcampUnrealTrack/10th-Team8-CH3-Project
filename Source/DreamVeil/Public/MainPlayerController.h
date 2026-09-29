#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
//FKey를 값으로 받는 함수가 있어서 전방 선언으로는 안 됨
#include "InputCoreTypes.h"
#include "MainPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UUserWidget;
class UWidget;
class UHeadshotFeedbackWidget;
class USoundBase;

UCLASS()
class DREAMVEIL_API AMainPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	AMainPlayerController();

	//총성과 별개로 플레이어에게 들려주는 짧은 헤드샷 확인음. BP에서 다른 소리로 교체할 수 있음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Headshot")
	TObjectPtr<USoundBase> HeadshotSound;
	

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

	//LV4 클리어 제작진 화면. 비어 있거나 구형 위젯이면 기본 EndingCreditsWidget을 사용함.
	//커스텀 화면은 EndingCreditsWidget을 상속해서 지정할 수 있음.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UUserWidget> EndingWidgetClass;

	//항상 떠 있는 화면 체력 스태미나 꿈의 조각 표시용 WBP_HUD를 넣을 것
	//레벨이 시작될 때 자동으로 뜨고 메뉴 위젯과 달리 입력 모드를 바꾸지 않음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	//메뉴 위젯을 띄우고 마우스로 누를 수 있게 함 침대 컴퓨터 게임 오버가 전부 이걸 씀
	//이미 열린 메뉴가 있으면 닫고 새로 엶 두 개가 겹쳐서 마우스가 먹통이 되는 걸 막음
	UFUNCTION(BlueprintCallable, Category = "UI")
	UUserWidget* OpenMenuWidget(TSubclassOf<UUserWidget> MenuWidgetClass);

	//메뉴를 띄우면서 게임을 멈춤 증강 선택처럼 고르는 동안 맞으면 안 되는 화면이 씀
	//멈춘 상태는 CloseMenuWidget이 풀어줌
	UFUNCTION(BlueprintCallable, Category = "UI")
	UUserWidget* OpenMenuWidgetPaused(TSubclassOf<UUserWidget> MenuWidgetClass);

	//열려 있는 메뉴 위젯을 닫고 게임 입력으로 되돌림 위젯의 닫기 버튼이 부를 것
	UFUNCTION(BlueprintCallable, Category = "UI")
	void CloseMenuWidget();

	//지금 메뉴 위젯이 떠 있는지 인벤토리처럼 메뉴와 겹치면 안 되는 창이 열기 전에 물어볼 것
	//블루프린트가 따로 bool을 들고 있으면 안 되는 이유 메뉴를 닫는 건 C++(CloseMenuWidget)이라
	//블루프린트 쪽 값은 내려줄 사람이 없어서 한번 켜지면 계속 켜진 채로 남음
	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsMenuWidgetOpen() const;

protected:
	//배경음 조절 키를 묶음 Enhanced Input 에셋을 새로 만들지 않은 이유
	//배경음은 게임 조작이 아니라 어느 맵에서나 같게 동작해야 해서 매핑 컨텍스트에 넣을 이유가 없음
	virtual void SetupInputComponent() override;

private:
	//위아래 키로 배경음 크기 조절 두 키가 하는 일이 같아서 눌린 키로 방향만 가름
	void HandleBGMVolumeKey(FKey PressedKey);

	//M 키로 배경음 껐다 켰다
	void ToggleBGMMute();


public:

	//게임을 멈추거나 푼다 월드 시간 타이머 몬스터가 전부 같이 멈춤
	//SetGamePaused만 부르면 멈추기 직전에 들어온 이동 입력이 그대로 남아서 W를 누르고 있으면 계속 앞으로 감
	//그래서 누르고 있던 키를 버리고 이동 시선 입력까지 막음
	//멈추는 일을 여기 한 군데로 모은 이유 증강 선택 침대 컴퓨터가 전부 같은 방식으로 멈추고 같은 방식으로 풀려야 함
	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetGameSuspended(bool bSuspended);

	virtual void BeginPlay() override;

	// Timeout reuses the Game Over screen, without converting it into a Hard-mode death.
	bool ShowCorruptionGameOver();

	//게임을 멈추고 LV4 제작진 화면을 띄움. 종료 전에는 다른 메뉴가 덮을 수 없음.
	UFUNCTION(BlueprintCallable, Category = "UI")
	bool ShowEnding();
	UFUNCTION(BlueprintCallable, Category = "UI")
	void FinishEnding();
	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsCorruptionGameOver() const { return bCorruptionGameOver; }

protected:
	//조종할 폰이 정해질 때 불림 플레이어 캐릭터면 사망 이벤트를 구독함
	virtual void OnPossess(APawn* InPawn) override;
	//다른 폰으로 바뀌면 이전 캐릭터의 헤드샷 알림이 남지 않도록 해제함
	virtual void OnUnPossess() override;

private:
	//헤드샷 신호를 화면 표시와 2D 효과음으로 바꿈
	void ShowHeadshotFeedback();
	UPROPERTY(Transient)
	TObjectPtr<UHeadshotFeedbackWidget> HeadshotFeedbackWidget;
	//HUD의 조준점 위치를 헤드샷 표시에 전달함. HUD가 없어지면 참조도 자동으로 무효가 됨
	TWeakObjectPtr<UWidget> CrosshairWidget;

	//플레이어가 죽었을 때 게임 오버 화면을 띄우고 마우스로 버튼을 누를 수 있게 함
	UFUNCTION()
	void ShowGameOver();

	//누르고 있던 키를 버리고 쌓인 속도를 지움
	//메뉴가 뜨면 입력 모드가 UI로 바뀌어서 키를 떼는 신호가 캐릭터까지 오지 않음
	//그래서 W를 누른 채로 침대를 누르면 메뉴가 떠 있는 동안에도 계속 앞으로 감
	//멈추는 메뉴(증강)와 안 멈추는 메뉴(침대 컴퓨터)가 똑같이 이 처리를 쓰게 하려고 따로 뺌
	void ClearHeldInput();

	//지금 열려 있는 메뉴 위젯 닫을 때 필요해서 들고 있음 없으면 nullptr
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> MenuWidgetInstance;

	//메뉴를 띄우면서 게임을 멈춘 것이 이 컨트롤러인지 CloseMenuWidget이 자기가 멈춘 것만 풀게 하려고 기억함
	//이게 없으면 증강 선택이 멈춰둔 게임을 다른 메뉴가 닫히면서 풀어버려서 몬스터가 그대로 움직이고 타이머도 계속 감
	bool bMenuPaused = false;
	bool bGameOverOpen = false;
	bool bEndingOpen = false;
	bool bEndingCompletionRequested = false;
	bool bCorruptionGameOver = false;
};
