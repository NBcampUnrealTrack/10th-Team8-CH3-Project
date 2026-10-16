#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DreamEndlessLoadingWidget.generated.h"
class UMediaPlayer;
class UMediaTexture;
class UMaterialInterface;
class UMaterialInstanceDynamic;
UCLASS()
class DREAMVEIL_API UDreamEndlessLoadingWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure,Category="Loading") bool IsVideoPlaying() const;
    UFUNCTION(BlueprintPure,Category="Loading") float GetVideoTime() const;
    UFUNCTION(BlueprintPure,Category="Loading") bool IsTransitionComplete() const { return bTravelled; }
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry,float DeltaSeconds) override;
    virtual void NativeDestruct() override;
private:
    UPROPERTY() TObjectPtr<UMediaPlayer> Player;
    UPROPERTY() TObjectPtr<UMediaTexture> Texture;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> VideoMaterial;
    double Started=0,FirstPlaying=0;
    bool bTravelled=false,bWasPaused=false;
};
