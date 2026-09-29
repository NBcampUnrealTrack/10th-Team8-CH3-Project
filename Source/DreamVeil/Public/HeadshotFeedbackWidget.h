#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HeadshotFeedbackWidget.generated.h"

//기존 HUD 위에 헤드샷 X만 추가로 그림. 공격 판정과 효과음은 이 위젯이 담당하지 않음
UCLASS()
class DREAMVEIL_API UHeadshotFeedbackWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	//연속 적중 시 남은 시간을 초기화해서 새 헤드샷도 선명하게 보이게 함
	void ShowHit(UWidget* InCrosshairWidget);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	//HUD가 소유한 실제 조준점을 약하게 참조해 창 크기와 UI 배율 변경을 그대로 따라감
	TWeakObjectPtr<UWidget> CrosshairWidget;
	//표시할 때만 틱을 사용하고 연출이 끝나면 위젯을 숨김
	float RemainingTime = 0.0f;
};
