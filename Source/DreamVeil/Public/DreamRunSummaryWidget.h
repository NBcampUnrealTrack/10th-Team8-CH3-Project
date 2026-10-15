#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DreamRunSummaryWidget.generated.h"
UCLASS()
class DREAMVEIL_API UDreamRunSummaryWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UDreamRunSummaryWidget(const FObjectInitializer& Initializer);
    UFUNCTION(BlueprintCallable,Category="UI|Results")
    static void AttachSummary(UUserWidget* Host);
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
private:
    UFUNCTION() void Continue();
    bool bContinuing=false;
    UPROPERTY() TObjectPtr<class UTextBlock> Summary;
    UPROPERTY() TObjectPtr<class UFont> Font;
    UPROPERTY() TObjectPtr<class UTexture2D> Frame;
    UPROPERTY() TObjectPtr<class UTexture2D> ButtonTexture;
};
