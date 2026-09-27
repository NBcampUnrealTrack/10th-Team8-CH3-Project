#include "MainPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "MainPlayerCharacter.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/TextBlock.h"
#include "DreamVeilGameInstance.h"

AMainPlayerController::AMainPlayerController() : 
InputMappingContext(nullptr),
MoveAction(nullptr),
JumpAction(nullptr),
LookAction(nullptr),
SprintAction(nullptr),
FireAction(nullptr),
EquipPistolAction(nullptr),
EquipRifleAction(nullptr),
InteractAction(nullptr)
{

}

void AMainPlayerController::BeginPlay()
{
    Super::BeginPlay();

    //게임 오버 화면에서 UI 전용 입력으로 바꾼 채 레벨을 옮기면 입력 모드는 새 레벨에도 그대로 남아서 조작이 안 됨
    //컨트롤러는 레벨마다 새로 만들어지니 시작할 때 게임 입력으로 돌려놓음
    SetInputMode(FInputModeGameOnly());

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            if (InputMappingContext)
            {
                Subsystem->AddMappingContext(InputMappingContext, 0);
            }
        }
    }

    //체력 스태미나 꿈의 조각을 보여주는 화면은 레벨이 시작될 때 바로 띄움
    //메뉴와 달리 입력 모드를 바꾸지 않아서 띄운 채로 움직이고 쏠 수 있음
    if (HUDWidgetClass)
    {
        if (UUserWidget* HUDWidget = CreateWidget<UUserWidget>(this, HUDWidgetClass))
        {
            HUDWidget->AddToViewport();
        }
    }
}

//조종할 폰이 정해질 때
void AMainPlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    //플레이어가 죽으면 게임 오버 화면을 띄우도록 구독
    //플레이어는 죽었다고 알리기만 하고 화면을 띄우는 건 입력과 화면을 맡는 컨트롤러가 함
    //AddUnique라서 같은 폰을 다시 조종해도 두 번 뜨지 않음
    if (AMainPlayerCharacter* PlayerCharacter = Cast<AMainPlayerCharacter>(InPawn))
    {
        PlayerCharacter->OnPlayerDied.AddUniqueDynamic(this, &AMainPlayerController::ShowGameOver);
    }
}

//플레이어가 죽었을 때
void AMainPlayerController::ShowGameOver()
{
    if (bGameOverOpen) return;
    bCorruptionGameOver = false;
    bGameOverOpen = OpenMenuWidgetPaused(GameOverWidgetClass) != nullptr;

    //위젯을 못 띄웠으면 글자를 넣을 곳도 없음
    if (!MenuWidgetInstance) return;

    //맞아 죽은 경우와 잠식된 경우의 문구를 나눔 어떻게 끝났는지 플레이어가 알아야 함
    if (auto* Title = Cast<UTextBlock>(MenuWidgetInstance->GetWidgetFromName(TEXT("Text_Title"))))
        Title->SetText(NSLOCTEXT("DreamUI", "DeathTitle", "꿈속에서 쓰러졌습니다"));

    auto* GI = GetGameInstance<UDreamVeilGameInstance>();

    if (!GI) return;

    GI->HandleHardGameOver();

    if (auto* Info = Cast<UTextBlock>(MenuWidgetInstance->GetWidgetFromName(TEXT("Text_Info"))))
    {
        Info->SetText(GI->GetDifficulty() == EGameDifficulty::Hard
            ? NSLOCTEXT("DreamUI", "DeathHard", "체력이 모두 소진되었습니다.\n저장 파일과 진행도가 초기화되고 로비로 돌아갑니다.")
            : GI->GetDifficulty() == EGameDifficulty::Easy
            ? NSLOCTEXT("DreamUI", "DeathEasy", "체력이 모두 소진되었습니다.\n이번 꿈에서 얻은 보상은 유지하고 로비로 돌아갑니다.")
            : NSLOCTEXT("DreamUI", "DeathNormal", "체력이 모두 소진되었습니다.\n이번 꿈에서 얻은 보상은 잃고, 입장 전 상태로 로비에 돌아갑니다."));
    }
}

bool AMainPlayerController::ShowCorruptionGameOver()
{
    if (bGameOverOpen) return true;
    if (auto* GI = GetGameInstance<UDreamVeilGameInstance>()) GI->HandleHardGameOver();
    UUserWidget* Screen = OpenMenuWidgetPaused(GameOverWidgetClass);
    if (!Screen) return false;
    bCorruptionGameOver = true;
    bGameOverOpen = true;
    if (auto* Title = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_Title"))))
        Title->SetText(NSLOCTEXT("DreamUI", "CorruptionTitle", "꿈에 완전히 잠식되었습니다"));
    if (auto* Info = Cast<UTextBlock>(Screen->GetWidgetFromName(TEXT("Text_Info"))))
    {
        const auto* GI = GetGameInstance<UDreamVeilGameInstance>();
        Info->SetText(GI && GI->GetDifficulty() == EGameDifficulty::Hard
            ? NSLOCTEXT("DreamUI", "CorruptionHard", "잠식도가 100%에 닿았습니다.\n꿈이 당신을 놓아주지 않습니다.\n저장 파일과 진행도가 초기화되고 로비로 돌아갑니다.")
            : GI && GI->GetDifficulty() == EGameDifficulty::Easy
            ? NSLOCTEXT("DreamUI", "CorruptionEasy", "잠식도가 100%에 닿았습니다.\n꿈이 당신을 놓아주지 않습니다.\n이번 꿈에서 얻은 보상은 유지하고 로비로 돌아갑니다.")
            : NSLOCTEXT("DreamUI", "CorruptionNormal", "잠식도가 100%에 닿았습니다.\n꿈이 당신을 놓아주지 않습니다.\n이번 꿈에서 얻은 보상은 잃고, 입장 전 상태로 로비에 돌아갑니다."));
    }
    return true;
}

//메뉴 위젯을 띄움
UUserWidget* AMainPlayerController::OpenMenuWidget(TSubclassOf<UUserWidget> MenuWidgetClass)
{
    // Do not let pending augment/interaction events replace the terminal screen.
    if (bGameOverOpen) return nullptr;
    //위젯을 안 넣었으면 띄울 게 없음
    if (!MenuWidgetClass)
    {
        return nullptr;
    }

    //침대를 보다가 컴퓨터를 누르는 것처럼 메뉴가 겹치면 먼저 것을 닫음
    CloseMenuWidget();

    MenuWidgetInstance = CreateWidget<UUserWidget>(this, MenuWidgetClass);

    if (!MenuWidgetInstance)
    {
        return nullptr;
    }

    MenuWidgetInstance->AddToViewport();

    //입력 모드를 UI로 바꾸기 전에 눌린 키를 버림
    //순서가 중요함 UI로 바꾼 뒤에 버리면 이미 캐릭터가 이동 입력을 들고 있어서 늦음
    //멈추지 않는 메뉴(침대 컴퓨터)도 이 줄 덕분에 W를 누른 채 열어도 캐릭터가 서 있음
    ClearHeldInput();

    //버튼을 마우스로 누를 수 있게 UI 전용 입력으로 바꾸고 커서를 보여줌
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(MenuWidgetInstance->TakeWidget());
    SetInputMode(InputMode);

    bShowMouseCursor = true;

    //위젯 쪽에서 값을 채우거나 이벤트를 연결할 수 있게 돌려줌
    return MenuWidgetInstance;
}

//메뉴 위젯을 띄우면서 게임을 멈춤
UUserWidget* AMainPlayerController::OpenMenuWidgetPaused(TSubclassOf<UUserWidget> MenuWidgetClass)
{
	UUserWidget* MenuWidget = OpenMenuWidget(MenuWidgetClass);

	//띄우지 못했으면 멈추지 않음 안 그러면 풀 방법이 없어서 게임이 굳음
	if (!MenuWidget)
	{
		return nullptr;
	}

	//위젯은 기본적으로 멈춘 동안에도 입력을 받으므로 버튼이 그대로 눌림
	//멈춘 것이 나라고 표시해둠 CloseMenuWidget이 이 표시를 보고 풀어줌
	bMenuPaused = true;
	SetGameSuspended(true);

	return MenuWidget;
}

//게임을 멈추거나 푼다
void AMainPlayerController::ClearHeldInput()
{
	//누르고 있던 키를 버림 이걸 해야 키를 뗀 것으로 쳐서 이동 입력이 0이 됨
	FlushPressedKeys();

	//이미 쌓인 속도를 지움 입력만 끊으면 관성으로 잠깐 더 미끄러짐
	if (ACharacter* PlayerCharacter = Cast<ACharacter>(GetPawn()))
	{
		PlayerCharacter->GetCharacterMovement()->StopMovementImmediately();
	}
}

void AMainPlayerController::SetGameSuspended(bool bSuspended)
{
	//월드 시간을 멈춤 이러면 몬스터 움직임 공격 타이머(제한 시간 웨이브)가 전부 같이 멈춤
	//SetGamePaused는 플레이어 컨트롤러가 있어야 먹혀서 여기(컨트롤러)에서 부름
	UGameplayStatics::SetGamePaused(this, bSuspended);

	if (bSuspended)
	{
		//누르고 있던 키와 쌓인 속도를 지움 안 지우면 W를 누른 상태로 멈춰서 푸는 순간까지 이동 입력이 살아 있음
		ClearHeldInput();

		//UI만 보는 동안에는 캐릭터가 움직이거나 카메라가 돌지 않게 입력 자체를 막음
		//멈췄는데도 움직이는 일이 생기지 않도록 이중으로 걸어둠
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
		return;
	}

	//막아둔 입력을 되돌림 SetIgnore...Input은 겹쳐 부르면 횟수가 쌓이는 값이라 Reset으로 한 번에 0으로 만듦
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();
}

//열려 있는 메뉴 위젯을 닫음
void AMainPlayerController::CloseMenuWidget()
{
	//내가 멈춘 것만 풀어줌
	//무조건 풀면 증강 선택처럼 다른 쪽이 멈춰둔 게임까지 같이 풀려서 고르는 동안 몬스터가 움직임
	if (bMenuPaused)
	{
		bMenuPaused = false;
		SetGameSuspended(false);
	}

    if (MenuWidgetInstance)
    {
        MenuWidgetInstance->RemoveFromParent();
        MenuWidgetInstance = nullptr;
    }

    //메뉴를 닫으면 다시 캐릭터를 조작해야 하므로 게임 입력으로 되돌림
    SetInputMode(FInputModeGameOnly());

    bShowMouseCursor = false;
}

//지금 메뉴 위젯이 떠 있는지
bool AMainPlayerController::IsMenuWidgetOpen() const
{
    //열고 닫는 쪽이 들고 있는 값을 그대로 돌려줌 상태를 한 군데만 두면 어긋날 일이 없음
    return MenuWidgetInstance != nullptr;
}
