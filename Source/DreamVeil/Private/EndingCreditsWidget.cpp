#include "EndingCreditsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "InputCoreTypes.h"
#include "HAL/PlatformTime.h"
#include "MainPlayerController.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"

UEndingCreditsWidget::UEndingCreditsWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
	static ConstructorHelpers::FObjectFinder<UFont> Regular(TEXT("/Game/UI/Fonts/Pretendard-Regular_Font.Pretendard-Regular_Font"));
	static ConstructorHelpers::FObjectFinder<UFont> Bold(TEXT("/Game/UI/Fonts/Pretendard-SemiBold_Font.Pretendard-SemiBold_Font"));
	BodyFont = Regular.Object;
	NameFont = Bold.Object;
}

void UEndingCreditsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildCredits();
}

void UEndingCreditsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RollStartSeconds = FPlatformTime::Seconds();
	UpdateCreditRoll(0.0f);
}

void UEndingCreditsWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	//Slate continues ticking during the gameplay pause. Never use a paused world timer here.
	if (!IsDesignTime()) UpdateCreditRoll(float(FPlatformTime::Seconds() - RollStartSeconds));
}

void UEndingCreditsWidget::UpdateCreditRoll(float ElapsedSeconds)
{
	if (!CreditScroll) return;
	//2-second opening hold, then 42 design units/sec; the final thank-you remains centered.
	const float Offset = FMath::Clamp((ElapsedSeconds - 2.0f) * 42.0f, 0.0f, 2050.0f);
	CreditScroll->SetScrollOffset(Offset);
}

void UEndingCreditsWidget::BuildCredits()
{
	if (!WidgetTree || WidgetTree->RootWidget) return;
	const FLinearColor Gold(0.72f, 0.53f, 0.28f, 1.0f);
	const FLinearColor Ivory(0.88f, 0.86f, 0.80f, 1.0f);
	auto* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Credits_Backdrop"));
	FSlateBrush Solid;
	Solid.DrawAs = ESlateBrushDrawType::Image;
	Solid.TintColor = FLinearColor::White;
	Backdrop->SetBrush(Solid);
	Backdrop->SetBrushColor(FLinearColor::Black);
	Backdrop->SetPadding(FMargin(24.0f));
	WidgetTree->RootWidget = Backdrop;

	//Scale-to-fit keeps a stable reading speed and a fixed exit button at any aspect ratio.
	auto* Scale = WidgetTree->ConstructWidget<UScaleBox>();
	Scale->SetStretch(EStretch::ScaleToFit);
	Backdrop->SetContent(Scale);
	auto* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(1400.0f);
	Size->SetHeightOverride(900.0f);
	auto* ScaleSlot = CastChecked<UScaleBoxSlot>(Scale->AddChild(Size));
	ScaleSlot->SetHorizontalAlignment(HAlign_Center);
	ScaleSlot->SetVerticalAlignment(VAlign_Center);
	auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	Size->SetContent(Canvas);
	CreditScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Credits_Scroll"));
	FScrollBoxStyle ScrollStyle;
	ScrollStyle.TopShadowBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	ScrollStyle.BottomShadowBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	ScrollStyle.LeftShadowBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	ScrollStyle.RightShadowBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	CreditScroll->SetWidgetStyle(ScrollStyle);
	CreditScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	CreditScroll->SetAllowOverscroll(false);
	CreditScroll->SetAnimateWheelScrolling(false);
	CreditScroll->SetAllowRightClickDragScrolling(false);
	CreditScroll->SetVisibility(ESlateVisibility::HitTestInvisible);
	CreditScroll->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	auto* ScrollSlot = Canvas->AddChildToCanvas(CreditScroll);
	ScrollSlot->SetPosition(FVector2D(0, 0));
	ScrollSlot->SetSize(FVector2D(1400, 780));
	auto* RollSize = WidgetTree->ConstructWidget<USizeBox>();
	RollSize->SetHeightOverride(2830);
	auto* RollSlot = CastChecked<UScrollBoxSlot>(CreditScroll->AddChild(RollSize));
	RollSlot->SetHorizontalAlignment(HAlign_Fill);
	auto* RollCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	RollSize->SetContent(RollCanvas);

	auto Place = [Canvas](UWidget* Widget, float X, float Y, float W, float H)
	{
		auto* Slot = Canvas->AddChildToCanvas(Widget);
		Slot->SetPosition(FVector2D(X, Y));
		Slot->SetSize(FVector2D(W, H));
	};
	auto Text = [this, RollCanvas](FName Id, const TCHAR* Caption, float X, float Y, float W, float H,
		int32 FontSize, FLinearColor Color, bool bBold, ETextJustify::Type Align = ETextJustify::Left)
	{
		auto* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Id);
		Label->SetText(FText::FromString(Caption));
		UFont* Font = bBold ? NameFont.Get() : BodyFont.Get();
		Label->SetFont(Font ? FSlateFontInfo(Font, FontSize) : FCoreStyle::GetDefaultFontStyle("Regular", FontSize));
		Label->SetColorAndOpacity(Color);
		Label->SetJustification(Align);
		Label->SetAutoWrapText(true);
		auto* Slot = RollCanvas->AddChildToCanvas(Label);
		Slot->SetPosition(FVector2D(X, Y));
		Slot->SetSize(FVector2D(W, H));
	};
	Text(TEXT("Credits_Title"), TEXT("CREDITS"), 100, 260, 1200, 90, 48, Gold, true, ETextJustify::Center);
	struct FCredit { const TCHAR* Name; const TCHAR* Role; const TCHAR* Work; };
	const FCredit Credits[] = {
		{ TEXT("김다솔"), TEXT("TEAM LEAD"), TEXT("Monster & Boss AI · Behavior Trees\nWeapons · Game Mode · Headshot Feedback") },
		{ TEXT("정윤재"), TEXT("DEPUTY TEAM LEAD · GIT MASTER\nPROJECT INTEGRATION & QUALITY ASSURANCE"), TEXT("Combat Logic · Player · Monsters · Game Mode · Game Instance\nWeapons · Augments · Shop · Inventory · UI\nBGM & Sound · Particles") },
		{ TEXT("이미르"), TEXT(""), TEXT("Concept & Game Design · Level Design\nUI Widgets · Shop Inventory") },
		{ TEXT("김성민"), TEXT(""), TEXT("Player · Monsters · Animation · Visual Effects\nAssets · Weapons · Particles · Sound") }
	};
	for (int32 I = 0; I < UE_ARRAY_COUNT(Credits); ++I)
	{
		const float Y = 490.0f + I * 420.0f;
		Text(*FString::Printf(TEXT("Credits_Name_%d"), I), Credits[I].Name, 100, Y, 1200, 80, 44, Ivory, true, ETextJustify::Center);
		Text(*FString::Printf(TEXT("Credits_Role_%d"), I), Credits[I].Role, 100, Y + 90, 1200, 80, 23, Gold, true, ETextJustify::Center);
		Text(*FString::Printf(TEXT("Credits_Work_%d"), I), Credits[I].Work, 100, Y + 182, 1200, 150, 26, Ivory, false, ETextJustify::Center);
	}
	Text(TEXT("Credits_Thanks"), TEXT("THANK YOU FOR PLAYING"), 100, 2380, 1200, 120, 46, Gold, true, ETextJustify::Center);
	auto* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Credits_Return"));
	FButtonStyle Style;
	Style.Normal = Solid;
	Style.Normal.TintColor = FLinearColor(0.06f, 0.045f, 0.025f, 1);
	Style.Hovered = Solid;
	Style.Hovered.TintColor = FLinearColor(0.17f, 0.12f, 0.06f, 1);
	Style.Pressed = Solid;
	Style.Pressed.TintColor = FLinearColor(0.09f, 0.06f, 0.03f, 1);
	Button->SetStyle(Style);
	Place(Button, 480, 818, 440, 62);
	auto* Caption = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Credits_ReturnCaption"));
	Caption->SetText(FText::FromString(TEXT("RETURN TO LOBBY")));
	Caption->SetFont(BodyFont ? FSlateFontInfo(BodyFont.Get(), 23) : FCoreStyle::GetDefaultFontStyle("Regular", 23));
	Caption->SetColorAndOpacity(Ivory);
	Button->SetContent(Caption);
	Button->OnClicked.AddUniqueDynamic(this, &UEndingCreditsWidget::ReturnToLobby);
}

FReply UEndingCreditsWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (!KeyEvent.IsRepeat() && (KeyEvent.GetKey() == EKeys::Escape || KeyEvent.GetKey() == EKeys::Enter))
	{
		ReturnToLobby();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, KeyEvent);
}

void UEndingCreditsWidget::ReturnToLobby()
{
	if (auto* Controller = Cast<AMainPlayerController>(GetOwningPlayer())) Controller->FinishEnding();
}
