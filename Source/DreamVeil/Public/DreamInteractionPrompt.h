#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DreamInteractionPrompt.generated.h"
class UTextBlock;
class AInteractableActorBase;

UCLASS()
class DREAMVEIL_API UDreamInteractionPrompt : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure,Category="Interaction") AInteractableActorBase* GetPromptTarget() const;
    UFUNCTION(BlueprintPure,Category="Interaction") FText GetPromptText() const;
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry,float DeltaSeconds) override;
private:
    void RefreshPrompt();
    UPROPERTY() TObjectPtr<UTextBlock> PromptText;
    TWeakObjectPtr<AInteractableActorBase> Target;
    float RefreshDelay=0;
    bool bShowing=false;
};
