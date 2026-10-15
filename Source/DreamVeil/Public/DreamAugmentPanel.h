#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "AugmentTypes.h"
#include "DreamAugmentPanel.generated.h"
UCLASS()
class DREAMVEIL_API UDreamRerollGlyph : public UWidget
{
    GENERATED_BODY()
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};
class UDreamAugmentPanel;
class UTextBlock;
class UVerticalBox;
class UHorizontalBox;
class UProgressBar;
class UImage;
class UBorder;
class UTexture2D;
class UFont;
class UDreamAugmentIconSet;

// One reusable button type avoids transient per-frame delegates and supports keyboard focus.
UCLASS()
class DREAMVEIL_API UDreamAugmentActionButton : public UButton
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UDreamAugmentPanel> Host;
    int32 Action=0;
    void Configure(UDreamAugmentPanel* Panel,int32 InAction);
    UFUNCTION() void Invoke();
};

UENUM()
enum class EDreamAugmentPanelMode:uint8 { Guide, ChoiceToolbar, SkillHUD };

UCLASS()
class DREAMVEIL_API UDreamAugmentPanel : public UUserWidget
{
    GENERATED_BODY()
public:
    UDreamAugmentPanel(const FObjectInitializer& Initializer);
    static void InstallChoiceToolbar(UUserWidget* Host);
    static UDreamAugmentPanel* OpenGuide(APlayerController* PC,UUserWidget* ReturnTo=nullptr);
    static void InstallSkillHUD(APlayerController* PC);
    UFUNCTION(BlueprintCallable, Category="UI|Augment") void HandleAction(int32 Action);
    UFUNCTION(BlueprintPure, Category="UI") FString GetSummary() const;
    UPROPERTY() EDreamAugmentPanelMode Mode=EDreamAugmentPanelMode::Guide;
    UPROPERTY() TObjectPtr<UUserWidget> ReturnWidget;
protected:
    virtual void NativeOnInitialized() override;
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry,float DeltaSeconds) override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
private:
    void Build();
    void RefreshGuide();
    void RefreshChoice();
    void RefreshHUD();
    void CloseGuide();
    void RefreshOwned();
    UTextBlock* Label(const FString& Text,int32 Size=18);
    UDreamAugmentActionButton* Button(UVerticalBox* Box,const FString& Text,int32 Action,bool Enabled=true);
    UPROPERTY() TObjectPtr<UVerticalBox> Body;
    UPROPERTY() TObjectPtr<UTextBlock> Header;
    UPROPERTY() TObjectPtr<UTextBlock> Status;
    UPROPERTY() TObjectPtr<UDreamAugmentActionButton> Reroll;
    UPROPERTY() TObjectPtr<UTextBlock> RerollLabel;
    UPROPERTY() TObjectPtr<UTextBlock> HUDText;
    UPROPERTY() TObjectPtr<UBorder> HealthCapMarker;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> SkillLabels;
    UPROPERTY() TArray<TObjectPtr<UTextBlock>> SkillKeyLabels;
    UPROPERTY() TArray<TObjectPtr<UImage>> SkillIcons;
    UPROPERTY() TObjectPtr<UTexture2D> PanelTexture;
    UPROPERTY() TObjectPtr<UTexture2D> NormalTexture;
    UPROPERTY() TObjectPtr<UTexture2D> HoverTexture;
    UPROPERTY() TObjectPtr<UTexture2D> PressTexture;
    UPROPERTY() TObjectPtr<UFont> Font;
    UPROPERTY() TObjectPtr<UDreamAugmentIconSet> Icons;
    FString Summary;
    float RefreshDelay=0;
    int32 Page=0;
    int32 Filter=0;
    int32 Selection=-1;
    int32 PendingConfirmation=-1;
    TArray<EAugmentID> LastChoices;
    TWeakObjectPtr<APawn> LastHUDPawn;
    int32 LastHUDRevision=INDEX_NONE;
    double NextHUDDescriptionRefresh=0;
    int32 DisplayedCooldownSeconds[6]={-1,-1,-1,-1,-1,-1};
    TArray<FKey> DisplayedSkillKeys;
};

// Keeps catalog hotkeys available while the reward widget owns UI-only focus.
UCLASS(Blueprintable)
class DREAMVEIL_API UDreamAugmentChoiceHost : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
    virtual void NativeDestruct() override;
private:
    TWeakObjectPtr<UDreamAugmentPanel> Overlay;
};
