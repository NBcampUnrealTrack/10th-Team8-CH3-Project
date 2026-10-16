#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DreamBossHealthWidget.generated.h"
class ABossMonster;
class UProgressBar;
class UTextBlock;
class UBorder;
class UFont;
class UTexture2D;
UCLASS()
class DREAMVEIL_API UDreamBossHealthWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UDreamBossHealthWidget(const FObjectInitializer& Initializer);
    UFUNCTION(BlueprintPure,Category="UI|Boss") float GetDisplayedHealthPercent() const;
    UFUNCTION(BlueprintPure,Category="UI|Boss") ABossMonster* GetDisplayedBoss() const;
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry,float DeltaSeconds) override;
    virtual void NativeDestruct() override;
private:
    void RefreshBoss();
    UPROPERTY() TObjectPtr<UProgressBar> Health;
    UPROPERTY() TObjectPtr<UTextBlock> Caption;
    UPROPERTY() TObjectPtr<UBorder> Panel;
    UPROPERTY() TObjectPtr<UFont> Font;
    UPROPERTY() TObjectPtr<UTexture2D> Frame;
    TWeakObjectPtr<ABossMonster> Boss;
    uint32 BossGeneration=0;
    float NextSearch=0;
    FTimerHandle RefreshTimer;
};
