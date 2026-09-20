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
    //게임 오버도 메뉴 위젯과 띄우는 방법이 같아서 같은 함수를 씀
    OpenMenuWidget(GameOverWidgetClass);
}

//메뉴 위젯을 띄움
UUserWidget* AMainPlayerController::OpenMenuWidget(TSubclassOf<UUserWidget> MenuWidgetClass)
{
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

    //버튼을 마우스로 누를 수 있게 UI 전용 입력으로 바꾸고 커서를 보여줌
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(MenuWidgetInstance->TakeWidget());
    SetInputMode(InputMode);

    bShowMouseCursor = true;

    //위젯 쪽에서 값을 채우거나 이벤트를 연결할 수 있게 돌려줌
    return MenuWidgetInstance;
}

//열려 있는 메뉴 위젯을 닫음
void AMainPlayerController::CloseMenuWidget()
{
    if (MenuWidgetInstance)
    {
        MenuWidgetInstance->RemoveFromParent();
        MenuWidgetInstance = nullptr;
    }

    //메뉴를 닫으면 다시 캐릭터를 조작해야 하므로 게임 입력으로 되돌림
    SetInputMode(FInputModeGameOnly());

    bShowMouseCursor = false;
}
