#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EndingCreditsWidget.generated.h"

class UFont;
class UScrollBox;

// Native UMG screen: available in packaged builds without a separately assigned Widget BP.
UCLASS()
class DREAMVEIL_API UEndingCreditsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UEndingCreditsWidget(const FObjectInitializer& ObjectInitializer);
	// Also used by the isolated editor preview; no gameplay state is changed here.
	void BuildCredits();
	// Deterministic scroll position, shared by the real-time playback and render verification.
	void UpdateCreditRoll(float ElapsedSeconds);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

private:
	UFUNCTION()
	void ReturnToLobby();

	UPROPERTY()
	TObjectPtr<UFont> BodyFont;
	UPROPERTY()
	TObjectPtr<UFont> NameFont;
	UPROPERTY()
	TObjectPtr<UScrollBox> CreditScroll;
	double RollStartSeconds = 0.0;
};
