#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AugmentTypes.h"
#include "DreamPauseWidget.generated.h"

class UTexture2D;
class UFont;
class UTextBlock;
class UScrollBox;
class UHorizontalBox;
class UDreamAugmentIconSet;

UCLASS()
class DREAMVEIL_API UDreamPauseWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="UI")
    FText GetDisplayText() const;
    UFUNCTION(BlueprintPure, Category="UI")
    FString GetAugmentDisplayText() const { return AugmentSummary; }
    UDreamPauseWidget(const FObjectInitializer& Initializer);
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
    UFUNCTION() void ResumeGame();
    UFUNCTION() void OpenAugmentGuide();
    UFUNCTION() void QuitGame();
    void RefreshAugmentList();
    FString AugmentSummary;
    UPROPERTY() TMap<EAugmentID,TObjectPtr<UHorizontalBox>> CachedAugmentRows;
    UPROPERTY() TObjectPtr<UTextBlock> EmptyAugmentLabel;
    UPROPERTY() TObjectPtr<UScrollBox> AugmentScroll;
    UPROPERTY() TObjectPtr<UDreamAugmentIconSet> AugmentIcons;
    UPROPERTY() TObjectPtr<UTextBlock> StatText;
    UPROPERTY() TObjectPtr<UTexture2D> PanelTexture;
    UPROPERTY() TObjectPtr<UTexture2D> NormalTexture;
    UPROPERTY() TObjectPtr<UTexture2D> HoverTexture;
    UPROPERTY() TObjectPtr<UTexture2D> PressTexture;
    UPROPERTY() TObjectPtr<UFont> MenuFont;
};
