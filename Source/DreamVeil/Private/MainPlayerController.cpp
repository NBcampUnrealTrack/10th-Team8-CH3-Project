#include "MainPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "MainPlayerCharacter.h"
#include "Blueprint/UserWidget.h"

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
    //위젯을 안 넣었으면 띄울 게 없음
    if (!GameOverWidgetClass)
    {
        return;
    }

    UUserWidget* GameOverWidget = CreateWidget<UUserWidget>(this, GameOverWidgetClass);

    if (!GameOverWidget)
    {
        return;
    }

    //따로 변수에 들고 있지 않음 버튼을 누르면 레벨이 바뀌면서 위젯도 같이 사라짐
    GameOverWidget->AddToViewport();

    //버튼을 마우스로 누를 수 있게 UI 전용 입력으로 바꾸고 커서를 보여줌
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(GameOverWidget->TakeWidget());
    SetInputMode(InputMode);

    bShowMouseCursor = true;
}
