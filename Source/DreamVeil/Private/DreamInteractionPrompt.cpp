#include "DreamInteractionPrompt.h"
#include "MainPlayerCharacter.h"
#include "InteractableActorBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Styling/CoreStyle.h"

void UDreamInteractionPrompt::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Canvas;
    auto* Scale=WidgetTree->ConstructWidget<UScaleBox>();Scale->SetStretch(EStretch::ScaleToFit);Scale->SetStretchDirection(EStretchDirection::DownOnly);
    auto* PromptSlot=Canvas->AddChildToCanvas(Scale);PromptSlot->SetAnchors(FAnchors(.5f,.78f));PromptSlot->SetAlignment(FVector2D(.5f,1));PromptSlot->SetSize(FVector2D(480,44));
    PromptText=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("InteractionPromptText"));Scale->SetContent(PromptText);
    auto* Font=LoadObject<UFont>(nullptr,TEXT("/Game/UI/Fonts/Pretendard-SemiBold_Font.Pretendard-SemiBold_Font"));
    auto Style=Font?FSlateFontInfo(Font,20):FCoreStyle::GetDefaultFontStyle("Regular",20);Style.OutlineSettings.OutlineSize=1;
    PromptText->SetFont(Style);PromptText->SetText(NSLOCTEXT("DreamUI","EnterInteraction","F키를 눌러 입장"));PromptText->SetJustification(ETextJustify::Center);
    PromptText->SetColorAndOpacity(FLinearColor(.93f,.86f,.69f));PromptText->SetShadowColorAndOpacity(FLinearColor::Black);PromptText->SetShadowOffset(FVector2D(1,2));
}
void UDreamInteractionPrompt::NativeConstruct()
{
    Super::NativeConstruct();SetVisibility(ESlateVisibility::HitTestInvisible);SetRenderOpacity(0);bShowing=false;RefreshPrompt();
}
void UDreamInteractionPrompt::NativeTick(const FGeometry& Geometry,float DeltaSeconds)
{
    Super::NativeTick(Geometry,DeltaSeconds);RefreshDelay-=DeltaSeconds;if(RefreshDelay>0)return;RefreshDelay=.15f;RefreshPrompt();
}
void UDreamInteractionPrompt::RefreshPrompt()
{
    auto* Player=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());Target=Player?Player->FindInteractionTarget():nullptr;
    const bool Show=Target.IsValid();if(Show!=bShowing){bShowing=Show;SetRenderOpacity(Show?1.f:0.f);}
}
FText UDreamInteractionPrompt::GetPromptText() const{return bShowing&&PromptText?PromptText->GetText():FText::GetEmpty();}
AInteractableActorBase* UDreamInteractionPrompt::GetPromptTarget() const{return Target.Get();}
