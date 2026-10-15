#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WeaponTypes.h"
#include "DreamEnhancePreviewWidget.generated.h"
class UTextBlock;
UCLASS()
class DREAMVEIL_API UDreamEnhancePreviewWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="UI")
    FText GetDisplayText() const;
    static void Install(UUserWidget* Host);
    void RefreshPreview();
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
private:
    void Refresh();
    void BuildPreview();
    TWeakObjectPtr<UUserWidget> HostWidget;
    UPROPERTY() TObjectPtr<UTextBlock> PreviewText;
    float RefreshElapsed=0;
    int32 LastIndex=INDEX_NONE;
    int32 LastEnhance=INDEX_NONE;
    int32 LastShards=INDEX_NONE;
    EWeaponPartSlot LastSlot=EWeaponPartSlot::Muzzle;
    EWeaponPartTier LastTier=EWeaponPartTier::Level1;
};
