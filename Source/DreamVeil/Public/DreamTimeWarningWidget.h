#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DreamTimeWarningWidget.generated.h"
class UTextBlock;
class USoundBase;
class UFont;
UCLASS()
class DREAMVEIL_API UDreamTimeWarningWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="UI")
    FText GetDisplayText() const;
    UDreamTimeWarningWidget(const FObjectInitializer& Initializer);
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
private:
    UFUNCTION() void UpdateWarning();
    UPROPERTY() TObjectPtr<UTextBlock> WarningText;
    UPROPERTY() TObjectPtr<USoundBase> WarningSound;
    UPROPERTY() TObjectPtr<UFont> WarningFont;
    FTimerHandle RefreshTimer;
    int32 LastSecond=INDEX_NONE;
    bool bWarned30=false;
    bool bWarned10=false;
};
