#include "MainPlayerController.h"
#include "EnhancedInputSubsystems.h"

AMainPlayerController::AMainPlayerController() : 
InputMappingContext(nullptr),
MoveAction(nullptr),
JumpAction(nullptr),
LookAction(nullptr),
SprintAction(nullptr),
<<<<<<< HEAD
FireAction(nullptr)
=======
FireAction(nullptr),
EquipPistolAction(nullptr),
EquipRifleAction(nullptr)
>>>>>>> 2026-09-15-GameInstanceState-ETC
{

}

void AMainPlayerController::BeginPlay()
{
    Super::BeginPlay();
  
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
