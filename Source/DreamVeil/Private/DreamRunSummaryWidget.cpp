#include "DreamRunSummaryWidget.h"
#include "DreamPlayPolish.h"
#include "DreamVeilGameInstance.h"
#include "MainPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
UDreamRunSummaryWidget::UDreamRunSummaryWidget(const FObjectInitializer& I):Super(I)
{
    SetIsFocusable(true);
    static ConstructorHelpers::FObjectFinder<UFont> F(TEXT("/Game/UI/Fonts/Pretendard-SemiBold_Font"));Font=F.Object;
    static ConstructorHelpers::FObjectFinder<UTexture2D> T(TEXT("/Game/UI/DreamVeilSkin/Frames/T_Frame_Panel_9Slice")),B(TEXT("/Game/UI/DreamVeilSkin/Buttons/T_Button_Normal"));Frame=T.Object;ButtonTexture=B.Object;
}
void UDreamRunSummaryWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();if(WidgetTree->RootWidget)return;
    auto* Shade=WidgetTree->ConstructWidget<UBorder>();Shade->SetBrushColor(FLinearColor(.02f,.01f,.04f,.94f));Shade->SetPadding(FMargin(30));WidgetTree->RootWidget=Shade;
    auto* Scale=WidgetTree->ConstructWidget<UScaleBox>();Scale->SetStretch(EStretch::ScaleToFit);Shade->SetContent(Scale);
    auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(780);Size->SetHeightOverride(520);Scale->SetContent(Size);
    auto* Border=WidgetTree->ConstructWidget<UBorder>();FSlateBrush Brush;Brush.SetResourceObject(Frame);Brush.DrawAs=ESlateBrushDrawType::Box;Brush.Margin=FMargin(.0625f);Border->SetBrush(Brush);Border->SetPadding(FMargin(36));Size->SetContent(Border);
    auto* Rows=WidgetTree->ConstructWidget<UVerticalBox>();Border->SetContent(Rows);
    auto* Title=WidgetTree->ConstructWidget<UTextBlock>();Title->SetText(FText::FromString(TEXT("전투 결과")));Title->SetFont(FSlateFontInfo(Font.Get(),30));Title->SetColorAndOpacity(FLinearColor(.93f,.83f,.62f));Rows->AddChildToVerticalBox(Title)->SetPadding(FMargin(0,0,0,20));
    auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();Rows->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Summary=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("EncounterSummary"));Summary->SetFont(FSlateFontInfo(Font.Get(),20));Summary->SetAutoWrapText(true);Summary->SetColorAndOpacity(FLinearColor(.9f,.86f,.96f));Scroll->AddChild(Summary);
    auto* Button=WidgetTree->ConstructWidget<UButton>();FButtonStyle Style;FSlateBrush B;B.SetResourceObject(ButtonTexture);B.DrawAs=ESlateBrushDrawType::Box;B.Margin=FMargin(.12f);Style.Normal=Style.Hovered=Style.Pressed=B;Button->SetStyle(Style);auto* ActionSize=WidgetTree->ConstructWidget<USizeBox>();ActionSize->SetHeightOverride(54);ActionSize->SetContent(Button);Rows->AddChildToVerticalBox(ActionSize)->SetPadding(FMargin(0,20,0,0));
    auto* Caption=WidgetTree->ConstructWidget<UTextBlock>();Caption->SetText(FText::FromString(TEXT("계속하기")));Caption->SetFont(FSlateFontInfo(Font.Get(),22));Button->SetContent(Caption);Button->OnClicked.AddUniqueDynamic(this,&UDreamRunSummaryWidget::Continue);
}
void UDreamRunSummaryWidget::NativeConstruct(){Super::NativeConstruct();bContinuing=false;if(auto* GI=GetGameInstance())if(auto* P=GI->GetSubsystem<UDreamPlayPreferences>())Summary->SetText(FText::FromString(P->GetLastReportText()));SetKeyboardFocus();}
FReply UDreamRunSummaryWidget::NativeOnKeyDown(const FGeometry& G,const FKeyEvent& E)
{if(E.GetKey()==EKeys::Enter){if(!E.IsRepeat())Continue();return FReply::Handled();}return Super::NativeOnKeyDown(G,E);}
void UDreamRunSummaryWidget::Continue()
{
    if(bContinuing)return;bContinuing=true;auto* PC=Cast<AMainPlayerController>(GetOwningPlayer());auto* GI=GetGameInstance<UDreamVeilGameInstance>();if(!PC||!GI)return;
    PC->bRunSummaryOpen=false;PC->CloseMenuWidget();if(GI->GetCurrentLevelNumber()==4&&PC->ShowEnding())return;GI->CompleteCurrentLevel();
}
void UDreamRunSummaryWidget::AttachSummary(UUserWidget* Host)
{
    // Existing game-over buttons and progression policy remain owned by the original screen.
    if(!Host||!Host->WidgetTree)return;
    auto* Canvas=Cast<UCanvasPanel>(Host->GetRootWidget());
    if(!Canvas)
    {
        // Responsive screens wrap the original canvas in ScaleBox/SizeBox.
        // Add to that existing canvas without replacing the cached Slate root.
        TArray<UWidget*> Widgets;Host->WidgetTree->GetAllWidgets(Widgets);
        for(auto* Widget:Widgets)if(auto* Inner=Cast<UCanvasPanel>(Widget)){Canvas=Inner;break;}
    }
    auto* GI=Host->GetGameInstance();if(!Canvas||!GI)return;
    auto* Pref=GI->GetSubsystem<UDreamPlayPreferences>();if(!Pref||!Pref->bHasReport)return;
    if(auto* Existing=Cast<UTextBlock>(Host->GetWidgetFromName(TEXT("EncounterSummaryText")))){Existing->SetText(FText::FromString(Pref->GetLastReportText()));return;}
    auto* Border=Host->WidgetTree->ConstructWidget<UBorder>();
    FSlateBrush FrameBrush;FrameBrush.SetResourceObject(LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/DreamVeilSkin/Frames/T_Frame_Panel_9Slice")));FrameBrush.ImageSize=FVector2D(128,128);FrameBrush.DrawAs=ESlateBrushDrawType::Box;FrameBrush.Margin=FMargin(.0625f);Border->SetBrush(FrameBrush);Border->SetPadding(FMargin(10));
    auto* Fill=Host->WidgetTree->ConstructWidget<UBorder>();Fill->SetBrushColor(FLinearColor(.025f,.015f,.04f,.96f));Fill->SetPadding(FMargin(12));Border->SetContent(Fill);
    auto* Scale=Host->WidgetTree->ConstructWidget<UScaleBox>();Scale->SetStretch(EStretch::ScaleToFit);Fill->SetContent(Scale);
    auto* Text=Host->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("EncounterSummaryText"));Text->SetText(FText::FromString(Pref->GetLastReportText()));Text->SetAutoWrapText(true);Text->SetWrapTextAt(500);Text->SetFont(FSlateFontInfo(LoadObject<UFont>(nullptr,TEXT("/Game/UI/Fonts/Pretendard-SemiBold_Font")),17));Text->SetColorAndOpacity(FLinearColor(.9f,.82f,1));Scale->SetContent(Text);
    auto* Slot=Canvas->AddChildToCanvas(Border);Slot->SetAnchors(FAnchors(.74f,.06f,.98f,.36f));Slot->SetOffsets(FMargin(0));Slot->SetZOrder(30);Border->SetVisibility(ESlateVisibility::HitTestInvisible);
}

