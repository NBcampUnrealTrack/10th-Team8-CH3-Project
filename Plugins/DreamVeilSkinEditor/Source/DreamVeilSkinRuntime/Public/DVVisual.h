#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DVVisual.generated.h"

// Read-only visual adapter. Never equips, buys, sells, enhances or changes game data.
UCLASS()
class DREAMVEILSKINRUNTIME_API UDVVisual : public UUserWidget {
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,Category="DreamVeil") FName Mode=TEXT("Part");
 UPROPERTY(EditAnywhere,Category="DreamVeil") int32 PreviewSlot=0;
 UPROPERTY(EditAnywhere,Category="DreamVeil") int32 PreviewTier=0;
 UPROPERTY(EditAnywhere,Category="DreamVeil") int32 WeaponIndex=0;
 UPROPERTY(EditAnywhere,Category="DreamVeil") bool bShowTier=true;
 UPROPERTY(EditAnywhere,Category="DreamVeil") int32 TimerFontSize=30;
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual void NativePreConstruct() override;
 virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
private:
 UPROPERTY(Transient) TObjectPtr<class UImage> Icon;
 UPROPERTY(Transient) TObjectPtr<class UImage> TierFrame;
 UPROPERTY(Transient) TObjectPtr<class UTextBlock> TimerText;
 float Elapsed=0;
 int32 LastSlot=-999,LastTier=-999;
 void RefreshVisual();
};
