#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "DreamLobbyWidgets.generated.h"
class UVerticalBox;class UHorizontalBox;class UTextBlock;class UFont;class UTexture2D;class UDreamLobbyPanel;
UCLASS()
class DREAMVEIL_API UDreamLobbyButton:public UButton
{
    GENERATED_BODY()
public:
    void Configure(UDreamLobbyPanel* Owner,int32 Command);
private:
    UPROPERTY() TObjectPtr<UDreamLobbyPanel> Panel;
    int32 Action=0;
    UFUNCTION() void Invoke();
protected:
    virtual void OnSlotAdded(UPanelSlot* AddedSlot) override;
};
UCLASS(Abstract)
class DREAMVEIL_API UDreamLobbyPanel:public UUserWidget
{
    GENERATED_BODY()
public:
    UDreamLobbyPanel(const FObjectInitializer& Initializer);
    UFUNCTION(BlueprintCallable,Category="UI") virtual void RefreshPanel();
    virtual void HandleAction(int32 Action);
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
    UTextBlock* Label(const FString& Text,int32 Size=20);
    UDreamLobbyButton* Button(UVerticalBox* Box,const FString& Text,int32 Action,bool Enabled=true);
    UHorizontalBox* PartRow(const FString& Text,UTexture2D* Icon);
    UTexture2D* PartIcon(int32 Slot) const;
    UPROPERTY() TObjectPtr<UVerticalBox> Content;
    UPROPERTY() TObjectPtr<UTextBlock> Title;
    UPROPERTY() TObjectPtr<UTextBlock> Wallet;
    UPROPERTY() TObjectPtr<UTextBlock> Status;
    UPROPERTY() TObjectPtr<UFont> Font;
    UPROPERTY() TObjectPtr<UTexture2D> Frame;
    UPROPERTY() TObjectPtr<UTexture2D> Normal;
    UPROPERTY() TObjectPtr<UTexture2D> Hover;
    UPROPERTY() TObjectPtr<UTexture2D> Press;
    TWeakObjectPtr<class UInventoryComponent> Inventory;
};
UCLASS()
class DREAMVEIL_API UDreamTrainingWidget:public UDreamLobbyPanel
{
    GENERATED_BODY()
public:
    virtual void RefreshPanel() override;
    virtual void HandleAction(int32 Action) override;
private:
    int32 SelectedAugment=-1;
};
UCLASS()
class DREAMVEIL_API UDreamInventoryWidget:public UDreamLobbyPanel
{
    GENERATED_BODY()
public:
    static void Install(UUserWidget* Host);
    virtual void RefreshPanel() override;
    virtual void HandleAction(int32 Action) override;
    UFUNCTION(BlueprintPure,Category="Inventory") int32 GetVisibleOwnedCount()const;
};
