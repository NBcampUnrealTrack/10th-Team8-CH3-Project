#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AugmentTypes.h"
#include "DreamUIBlueprintLibrary.generated.h"

class UUserWidget;
class UTexture2D;

// Hard asset references ensure all twelve icons are included when cooking the UI.
UCLASS(BlueprintType)
class DREAMVEIL_API UDreamAugmentIconSet : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Augment")
    TMap<EAugmentID, TObjectPtr<UTexture2D>> Icons;
};

UCLASS()
class DREAMVEIL_API UDreamUIBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Dream UI", meta=(DefaultToSelf="Widget"))
    static void OpenBedDreamSelect(UUserWidget* Widget);

    //메인 메뉴 배경음을 틂 WBP_MainMenu의 Construct가 부름
    //메인 메뉴 맵만 엔진 기본 게임모드를 써서 AMainGameModeBase가 돌지 않기 때문
    UFUNCTION(BlueprintCallable, Category = "DreamVeil|BGM", meta = (DefaultToSelf = "Widget"))
    static void PlayMainMenuBGM(UUserWidget* Widget);
    UFUNCTION(BlueprintCallable, Category="Dream UI", meta=(DefaultToSelf="Widget"))
    static void CloseBedDreamMenu(UUserWidget* Widget);
    UFUNCTION(BlueprintCallable, Category="Dream UI", meta=(DefaultToSelf="Widget"))
    static void RefreshDreamSelection(UUserWidget* Widget);
    // LevelNumber 0 selects Endless. The game instance remains the unlock authority.
    UFUNCTION(BlueprintCallable, Category="Dream UI", meta=(DefaultToSelf="Widget"))
    static bool EnterDream(UUserWidget* Widget, int32 LevelNumber);
    UFUNCTION(BlueprintCallable, Category="Dream UI", meta=(DefaultToSelf="Widget"))
    static void ApplyAugmentIcons(UUserWidget* Widget, const TArray<EAugmentID>& Choices, UDreamAugmentIconSet* IconSet);
    UFUNCTION(BlueprintCallable, Category="Dream UI", meta=(DefaultToSelf="Widget"))
    static void ContinueDreamGameOver(UUserWidget* Widget);
};
