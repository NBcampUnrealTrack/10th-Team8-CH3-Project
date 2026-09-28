#include "HeadshotFeedbackWidget.h"
#include "MainPlayerCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/CanvasPanel.h"
#include "Rendering/DrawElements.h"

namespace
{
	//짧게 강조한 뒤 사라져 조준점을 오래 가리지 않게 함
	constexpr float HeadshotDuration = 0.2f;
}

void UHeadshotFeedbackWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	//BP 에셋을 추가로 요구하지 않고 전체 화면 위에 그릴 공간만 만듦
	WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>();
	//페이드와 이동 좌표가 매 프레임 바뀌므로 Slate가 이전 그림을 캐시하지 않게 함
	ForceVolatile(true);
	SetVisibility(ESlateVisibility::Hidden);
}

void UHeadshotFeedbackWidget::ShowHit()
{
	RemainingTime = HeadshotDuration;
	//표시는 하되 마우스나 게임 입력을 가로채지 않음
	SetVisibility(ESlateVisibility::HitTestInvisible);
	InvalidateLayoutAndVolatility();
}

void UHeadshotFeedbackWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RemainingTime = FMath::Max(0.0f, RemainingTime - InDeltaTime);
	if (RemainingTime <= 0.0f) SetVisibility(ESlateVisibility::Hidden);
}

int32 UHeadshotFeedbackWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const AMainPlayerCharacter* Player = Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());
	FVector2D ScreenPosition;
	if (RemainingTime <= 0.0f || !Player || !Player->GetCrosshairScreenPosition(ScreenPosition)) return BaseLayer;

	//크로스헤어가 실제 탄착점을 따라 움직이므로 화면 중앙에 고정하지 않고 같은 좌표를 사용함
	//픽셀 좌표를 위젯 좌표로 변환해서 DPI 배율과 창 크기가 달라도 위치가 맞게 함
	FVector2D Center;
	USlateBlueprintLibrary::ScreenToWidgetLocal(GetOwningPlayer(), AllottedGeometry, ScreenPosition, Center);
	const float Elapsed = HeadshotDuration - RemainingTime;
	const float Scale = FMath::Lerp(1.2f, 1.0f, FMath::Clamp(Elapsed / 0.05f, 0.0f, 1.0f));
	const float Alpha = FMath::Clamp(RemainingTime / 0.15f, 0.0f, 1.0f);
	const FLinearColor Color(1.0f, 0.08f, 0.08f, Alpha);
	//네 대각선을 따로 그려 정중앙을 비움. 조준점과 적의 머리를 가리지 않게 하기 위함
	for (const FVector2D Direction : { FVector2D(-1, -1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(1, 1) })
	{
		const TArray<FVector2D> Points { Center + Direction * (7.0f * Scale), Center + Direction * (15.0f * Scale) };
		FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 1, AllottedGeometry.ToPaintGeometry(),
			Points, ESlateDrawEffect::None, Color, true, 2.5f);
	}
	return BaseLayer + 1;
}
