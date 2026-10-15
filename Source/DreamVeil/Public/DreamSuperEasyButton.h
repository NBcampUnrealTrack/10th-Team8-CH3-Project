#pragma once
#include "CoreMinimal.h"
#include "Components/Button.h"
#include "DreamVeilGameInstance.h"
#include "DreamSuperEasyButton.generated.h"
class UUserWidget;
UCLASS()
class DREAMVEIL_API UDreamSuperEasyButton : public UButton
{
    GENERATED_BODY()
public:
    static void Install(UUserWidget* Host);
    UFUNCTION(BlueprintCallable, Category="UI|Difficulty") void SelectSuperEasy();
private:
    UFUNCTION() void RefreshSelection(EGameDifficulty Difficulty);
    TWeakObjectPtr<UUserWidget> HostWidget;
};
