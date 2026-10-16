#include "DreamAugmentPanel.h"
#include "DispatchTableComponent.h"
#include "DreamAugmentCatalog.h"
#include "DreamAugmentRuntime.h"
#include "DreamAugmentFeedback.h"
#include "DreamPlayPolish.h"
#include "Components/EditableTextBox.h"
#include "MainGameModeBase.h"
#include "Components/ButtonSlot.h"
#include "MainPlayerCharacter.h"
#include "MainPlayerController.h"
#include "CombatStatsComponent.h"
#include "DreamPauseWidget.h"
#include "DreamUIBlueprintLibrary.h"
#include "DreamVeilGameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Components/Border.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"
#include "Styling/CoreStyle.h"
#include "Kismet/GameplayStatics.h"

void UDreamAugmentActionButton::Configure(UDreamAugmentPanel* Panel,int32 InAction)
{ Host=Panel;Action=InAction;OnClicked.AddUniqueDynamic(this,&UDreamAugmentActionButton::Invoke); }
void UDreamAugmentActionButton::Invoke() { if(Host) Host->HandleAction(Action); }

UDreamAugmentPanel::UDreamAugmentPanel(const FObjectInitializer& Initializer):Super(Initializer)
{
    SetIsFocusable(true);
    static ConstructorHelpers::FObjectFinder<UTexture2D> Panel(TEXT("/Game/UI/DreamVeilSkin/Frames/T_Frame_Panel_9Slice"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> Normal(TEXT("/Game/UI/DreamVeilSkin/Buttons/T_Button_Normal"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> Hover(TEXT("/Game/UI/DreamVeilSkin/Buttons/T_Button_Hovered"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> Press(TEXT("/Game/UI/DreamVeilSkin/Buttons/T_Button_Pressed"));
    static ConstructorHelpers::FObjectFinder<UFont> Typeface(TEXT("/Game/UI/Fonts/Pretendard-SemiBold_Font"));
    static ConstructorHelpers::FObjectFinder<UDreamAugmentIconSet> IconSet(TEXT("/Game/UI/AugmentIcons/DA_AugmentIcons"));
    PanelTexture=Panel.Object;NormalTexture=Normal.Object;HoverTexture=Hover.Object;PressTexture=Press.Object;Font=Typeface.Object;Icons=IconSet.Object;
}
UTextBlock* UDreamAugmentPanel::Label(const FString& Text,int32 Size)
{
    auto* W=WidgetTree->ConstructWidget<UTextBlock>(); W->SetText(FText::FromString(Text));
    W->SetFont(Font?FSlateFontInfo(Font.Get(),Size):FCoreStyle::GetDefaultFontStyle("Regular",Size));
    W->SetColorAndOpacity(FLinearColor(.93f,.86f,.69f));W->SetAutoWrapText(true);return W;
}
UDreamAugmentActionButton* UDreamAugmentPanel::Button(UVerticalBox* Box,const FString& Text,int32 Action,bool Enabled)
{
    auto* W=WidgetTree->ConstructWidget<UDreamAugmentActionButton>();W->Configure(this,Action);
    auto Brush=[](UTexture2D* T){FSlateBrush B;B.SetResourceObject(T);B.DrawAs=ESlateBrushDrawType::Box;B.Margin=FMargin(.12f);return B;};
    FButtonStyle Style;Style.Normal=Brush(NormalTexture);Style.Hovered=Brush(HoverTexture);Style.Pressed=Brush(PressTexture);W->SetStyle(Style);
    auto* Caption=Label(Text,20);Caption->SetJustification(ETextJustify::Center);
    Caption->SetAutoWrapText(false);
    if(Action>=1000&&Action<1256)
    {
        Style.Normal=FSlateColorBrush(FLinearColor::Transparent);
        Style.Hovered=FSlateColorBrush(FLinearColor(.2f,.10f,.29f,1));
        Style.Pressed=FSlateColorBrush(FLinearColor(.28f,.14f,.4f,1));W->SetStyle(Style);
        Caption->SetJustification(ETextJustify::Left);Caption->SetAutoWrapText(false);Caption->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);Style.NormalPadding=Style.PressedPadding=FMargin(12,6);W->SetStyle(Style);
    }
    W->SetContent(Caption);
    if(Action>=1000)if(auto* CaptionSlot=Cast<UButtonSlot>(Caption->Slot))CaptionSlot->SetHorizontalAlignment(HAlign_Fill);
    W->SetIsEnabled(Enabled);
    auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetMinDesiredHeight(Action>=1000||(Action>=20&&Action<=24)?44:Mode==EDreamAugmentPanelMode::ChoiceToolbar?48:54);Size->SetContent(W);
    Box->AddChildToVerticalBox(Size)->SetPadding(FMargin(0,3));return W;
}
void UDreamAugmentPanel::NativeOnInitialized() { Super::NativeOnInitialized(); }
TSharedRef<SWidget> UDreamAugmentPanel::RebuildWidget()
{
    // Mode is assigned after CreateWidget but before attaching. Build before Slate caches its root.
    if(WidgetTree && !WidgetTree->RootWidget) Build();
    return Super::RebuildWidget();
}
void UDreamAugmentPanel::NativeConstruct()
{
    Super::NativeConstruct();if(!WidgetTree->RootWidget) Build();
    if(Mode==EDreamAugmentPanelMode::Guide) { Page=0;Filter=0;Selection=-1;PendingConfirmation=-1;RefreshGuide();SetKeyboardFocus(); }
    if(Mode==EDreamAugmentPanelMode::ChoiceToolbar) RefreshChoice();
    if(Mode==EDreamAugmentPanelMode::SkillHUD) RefreshHUD();
}
void UDreamAugmentPanel::Build()
{
    auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("AugmentRoot"));WidgetTree->RootWidget=Canvas;
    if(Mode==EDreamAugmentPanelMode::ChoiceToolbar)
    {
        auto* Layout=WidgetTree->ConstructWidget<UVerticalBox>();auto* LayoutSlot=Canvas->AddChildToCanvas(Layout);LayoutSlot->SetAnchors(FAnchors(0,0,1,1));LayoutSlot->SetOffsets(FMargin(4));
        Reroll=WidgetTree->ConstructWidget<UDreamAugmentActionButton>();Reroll->Configure(this,10);Reroll->SetToolTipText(FText::FromString(TEXT("무료 리롤 · 이번 선택지 다시 뽑기 (선택마다 1회)")));
        // The frame is outside the disabled button: a spent reroll stays legible.
        auto* Frame=WidgetTree->ConstructWidget<UBorder>();
        Frame->SetBrush(FSlateColorBrush(FLinearColor(.72f,.55f,.29f,1.f)));Frame->SetPadding(FMargin(2));Frame->SetContent(Reroll);
        FButtonStyle Style;
        Style.Normal=FSlateColorBrush(FLinearColor(.035f,.025f,.045f,.98f));
        Style.Hovered=FSlateColorBrush(FLinearColor(.18f,.12f,.065f,1.f));
        Style.Pressed=FSlateColorBrush(FLinearColor(.09f,.055f,.025f,1.f));
        Style.Disabled=Style.Normal;Reroll->SetStyle(Style);
        auto* Square=WidgetTree->ConstructWidget<USizeBox>();Square->SetHeightOverride(78);Square->SetWidthOverride(78);Square->SetContent(Frame);Layout->AddChildToVerticalBox(Square)->SetHorizontalAlignment(HAlign_Center);
        auto* GlyphSize=WidgetTree->ConstructWidget<USizeBox>();GlyphSize->SetWidthOverride(46);GlyphSize->SetHeightOverride(46);GlyphSize->SetContent(WidgetTree->ConstructWidget<UDreamRerollGlyph>());Reroll->SetContent(GlyphSize);
        RerollLabel=Label(TEXT("1 / 1"),14);RerollLabel->SetJustification(ETextJustify::Center);Layout->AddChildToVerticalBox(RerollLabel);return;
    }
    if(Mode==EDreamAugmentPanelMode::SkillHUD)
    {
        auto* Scale=WidgetTree->ConstructWidget<UScaleBox>();Scale->SetStretch(EStretch::ScaleToFit);Scale->SetStretchDirection(EStretchDirection::DownOnly);
        auto* ScaleSlot=Canvas->AddChildToCanvas(Scale);ScaleSlot->SetAnchors(FAnchors(0,0,1,1));ScaleSlot->SetOffsets(FMargin(0));
        auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(396);Size->SetHeightOverride(76);Scale->SetContent(Size);
        auto* Slots=WidgetTree->ConstructWidget<UHorizontalBox>();Size->SetContent(Slots);
        for(int32 I=0;I<6;++I)
        {
            auto* Column=WidgetTree->ConstructWidget<UVerticalBox>();Slots->AddChildToHorizontalBox(Column)->SetPadding(FMargin(0,0,10,0));
            auto* IconSize=WidgetTree->ConstructWidget<USizeBox>();IconSize->SetWidthOverride(56);IconSize->SetHeightOverride(56);Column->AddChildToVerticalBox(IconSize);
            auto* Rim=WidgetTree->ConstructWidget<UBorder>();Rim->SetBrush(FSlateColorBrush(FLinearColor(.55f,.28f,.8f,.9f)));Rim->SetPadding(FMargin(1.5f));IconSize->SetContent(Rim);
            auto* Inner=WidgetTree->ConstructWidget<UBorder>();Inner->SetBrush(FSlateColorBrush(FLinearColor(.055f,.025f,.09f,.8f)));Inner->SetPadding(FMargin(3));Rim->SetContent(Inner);SkillFrames.Add(Rim);
            auto* Stack=WidgetTree->ConstructWidget<UOverlay>();Inner->SetContent(Stack);
            auto* Icon=WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),*FString::Printf(TEXT("SkillIcon%d"),I+1));
            auto* ImageSlot=Stack->AddChildToOverlay(Icon);ImageSlot->SetHorizontalAlignment(HAlign_Fill);ImageSlot->SetVerticalAlignment(VAlign_Fill);SkillIcons.Add(Icon);
            auto* Number=Label(TEXT(""),24);Number->SetAutoWrapText(false);Number->SetJustification(ETextJustify::Center);Number->SetShadowColorAndOpacity(FLinearColor::Black);Number->SetShadowOffset(FVector2D(2,2));
            auto* NumberSlot=Stack->AddChildToOverlay(Number);NumberSlot->SetHorizontalAlignment(HAlign_Center);NumberSlot->SetVerticalAlignment(VAlign_Center);SkillLabels.Add(Number);

        }
        FeedbackLabel=Label(TEXT(""),16);FeedbackLabel->SetAutoWrapText(false);FeedbackLabel->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
        FeedbackLabel->SetColorAndOpacity(FSlateColor(FLinearColor(.94f,.82f,1)));FeedbackLabel->SetShadowColorAndOpacity(FLinearColor::Black);FeedbackLabel->SetShadowOffset(FVector2D(1,1));
        auto* NoticeSlot=Canvas->AddChildToCanvas(FeedbackLabel);NoticeSlot->SetPosition(FVector2D(0,-32));NoticeSlot->SetSize(FVector2D(396,28));
        SetVisibility(ESlateVisibility::HitTestInvisible);return;
    }
    if(Mode==EDreamAugmentPanelMode::Guide)
    {
        auto* Shade=WidgetTree->ConstructWidget<UBorder>();Shade->SetBrushColor(FLinearColor(.005f,.003f,.009f,.88f));
        auto* ShadeSlot=Canvas->AddChildToCanvas(Shade);ShadeSlot->SetAnchors(FAnchors(0,0,1,1));ShadeSlot->SetOffsets(FMargin(0));
    }
    auto* Scale=WidgetTree->ConstructWidget<UScaleBox>();Scale->SetStretch(EStretch::ScaleToFit);Scale->SetStretchDirection(EStretchDirection::DownOnly);
    auto* ScaleSlot=Canvas->AddChildToCanvas(Scale);ScaleSlot->SetAnchors(FAnchors(0,0,1,1));ScaleSlot->SetOffsets(FMargin(Mode==EDreamAugmentPanelMode::Guide?16:0));
    auto* Size=WidgetTree->ConstructWidget<USizeBox>();Scale->SetContent(Size);
    Size->SetWidthOverride(Mode==EDreamAugmentPanelMode::Guide?980:Mode==EDreamAugmentPanelMode::SkillHUD?900:900);
    Size->SetHeightOverride(Mode==EDreamAugmentPanelMode::Guide?640:Mode==EDreamAugmentPanelMode::ChoiceToolbar?116:170);
    auto* Background=WidgetTree->ConstructWidget<UBorder>();Background->SetBrushColor(FLinearColor(.025f,.02f,.035f,.96f));Size->SetContent(Background);
    auto* Frame=WidgetTree->ConstructWidget<UBorder>();FSlateBrush B;B.SetResourceObject(PanelTexture);B.DrawAs=ESlateBrushDrawType::Box;B.Margin=FMargin(Mode==EDreamAugmentPanelMode::Guide?.0625f:.015625f);Frame->SetBrush(B);
    Frame->SetPadding(FMargin(Mode==EDreamAugmentPanelMode::Guide?26:10));Background->SetContent(Frame);
    auto* Rows=WidgetTree->ConstructWidget<UVerticalBox>();Frame->SetContent(Rows);
    Header=Label(TEXT(""),Mode==EDreamAugmentPanelMode::Guide?26:15);Rows->AddChildToVerticalBox(Header)->SetPadding(FMargin(0,0,0,4));
    if(Mode==EDreamAugmentPanelMode::Guide)
    {
        auto* Tabs=WidgetTree->ConstructWidget<UHorizontalBox>();Rows->AddChildToVerticalBox(Tabs);
        const TCHAR* Names[]={TEXT("증강 도감 · Q"),TEXT("진화 루트"),TEXT("보유 증강 · E"),TEXT("닫기 · ESC / Z")};
        for(int32 I=0;I<4;++I){auto* Col=WidgetTree->ConstructWidget<UVerticalBox>();Tabs->AddChildToHorizontalBox(Col)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Button(Col,Names[I],I==2?4:I);}
        Search=WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),TEXT("AugmentSearch"));Search->SetHintText(FText::FromString(TEXT("증강 이름 검색")));Search->WidgetStyle.TextStyle.SetFont(FSlateFontInfo(Font.Get(),17));Search->WidgetStyle.SetBackgroundImageNormal(FSlateColorBrush(FLinearColor(.04f,.02f,.07f,1))).SetBackgroundImageHovered(FSlateColorBrush(FLinearColor(.1f,.055f,.15f,1))).SetBackgroundImageFocused(FSlateColorBrush(FLinearColor(.1f,.055f,.15f,1))).SetForegroundColor(FLinearColor(.94f,.9f,1)).SetPadding(FMargin(10,5));Search->OnTextChanged.AddUniqueDynamic(this,&UDreamAugmentPanel::SearchChanged);Rows->AddChildToVerticalBox(Search)->SetPadding(FMargin(0,8));
        auto* Filters=WidgetTree->ConstructWidget<UHorizontalBox>();Rows->AddChildToVerticalBox(Filters);
        const TCHAR* FiltersText[]={TEXT("전체"),TEXT("패시브"),TEXT("액티브"),TEXT("보유"),TEXT("진화 가능")};
        for(int32 I=0;I<5;++I){auto* Col=WidgetTree->ConstructWidget<UVerticalBox>();Filters->AddChildToHorizontalBox(Col)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));Button(Col,FiltersText[I],20+I);}
        Status=Label(TEXT(""),16);Rows->AddChildToVerticalBox(Status)->SetPadding(FMargin(0,8));
        auto* Columns=WidgetTree->ConstructWidget<UHorizontalBox>();Rows->AddChildToVerticalBox(Columns)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();Scroll->SetAlwaysShowScrollbar(true);
        Columns->AddChildToHorizontalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        Body=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Body);
        auto* DetailFrame=WidgetTree->ConstructWidget<UBorder>();DetailFrame->SetBrush(FSlateColorBrush(FLinearColor(.055f,.035f,.08f,.9f)));DetailFrame->SetPadding(FMargin(18));
        auto* DetailColumn=Columns->AddChildToHorizontalBox(DetailFrame);DetailColumn->SetSize(FSlateChildSize(ESlateSizeRule::Fill));DetailColumn->SetPadding(FMargin(14,0,0,0));
        auto* DetailScroll=WidgetTree->ConstructWidget<UScrollBox>();DetailScroll->SetAlwaysShowScrollbar(true);DetailFrame->SetContent(DetailScroll);
        DetailBody=WidgetTree->ConstructWidget<UVerticalBox>();DetailScroll->AddChild(DetailBody);

    }

}
void UDreamAugmentPanel::NativeTick(const FGeometry& Geometry,float Delta)
{
    Super::NativeTick(Geometry,Delta);RefreshDelay-=Delta;if(RefreshDelay>0) return;RefreshDelay=.1f;
    if(Mode==EDreamAugmentPanelMode::SkillHUD) RefreshHUD();
    else if(Mode==EDreamAugmentPanelMode::ChoiceToolbar) RefreshChoice();
    else if(Search&&Search->GetText().ToString()!=SearchText)SearchChanged(Search->GetText());
}
void UDreamAugmentPanel::InstallChoiceToolbar(UUserWidget* Host)
{
    if(!Host) return;
    if(auto* Existing=Cast<UDreamAugmentPanel>(Host->GetWidgetFromName(TEXT("DV_AugmentToolbar")))) { Existing->LastChoices.Empty();Existing->RefreshChoice();return; }
    auto* Canvas=Cast<UCanvasPanel>(Host->GetRootWidget());
    if(!Canvas) Canvas=Cast<UCanvasPanel>(Host->GetWidgetFromName(TEXT("CanvasPanel_50")));
    if(!Canvas) return;
    auto* W=CreateWidget<UDreamAugmentPanel>(Host->GetOwningPlayer(),StaticClass(),TEXT("DV_AugmentToolbar"));
    W->Mode=EDreamAugmentPanelMode::ChoiceToolbar;W->ReturnWidget=Host;
    auto* Slot=Canvas->AddChildToCanvas(W);Slot->SetAnchors(FAnchors(.5f,1.f));Slot->SetAlignment(FVector2D(.5f,1.f));Slot->SetPosition(FVector2D(0,-14));Slot->SetSize(FVector2D(96,108));Slot->SetZOrder(100);
}
void UDreamAugmentPanel::InstallSkillHUD(APlayerController* PC)
{
    if(!PC || !PC->IsLocalController()) return;
    auto* W=CreateWidget<UDreamAugmentPanel>(PC);W->Mode=EDreamAugmentPanelMode::SkillHUD;
    W->AddToViewport(4);
    // UE resets anchors in SetDesiredSize/SetPosition. Apply bottom-left anchors LAST.
    W->SetDesiredSizeInViewport(FVector2D(396,76));W->SetPositionInViewport(FVector2D(20,-20),false);
    W->SetAlignmentInViewport(FVector2D(0.f,1.f));W->SetAnchorsInViewport(FAnchors(0.f,1.f));
}
UDreamAugmentPanel* UDreamAugmentPanel::OpenGuide(APlayerController* Controller,UUserWidget* ReturnTo)
{
    auto* PC=Cast<AMainPlayerController>(Controller);if(!PC) return nullptr;
    UDreamAugmentPanel* Guide=nullptr;
    if(ReturnTo)
    {
        Guide=CreateWidget<UDreamAugmentPanel>(PC);Guide->ReturnWidget=ReturnTo;Guide->AddToViewport(100);
        FInputModeUIOnly Input;Input.SetWidgetToFocus(Guide->TakeWidget());PC->SetInputMode(Input);Guide->SetKeyboardFocus();
    }
    else Guide=Cast<UDreamAugmentPanel>(PC->OpenMenuWidgetPaused(StaticClass()));
    return Guide;
}
void UDreamAugmentPanel::CloseGuide()
{
    auto* PC=Cast<AMainPlayerController>(GetOwningPlayer());if(!PC)return;
    if(ReturnWidget){auto* Target=ReturnWidget.Get();RemoveFromParent();FInputModeUIOnly Input;Input.SetWidgetToFocus(Target->TakeWidget());PC->SetInputMode(Input);Target->SetKeyboardFocus();}
    else PC->CloseMenuWidget();
}
FReply UDreamAugmentPanel::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if(Mode==EDreamAugmentPanelMode::Guide)
    {
        if(Event.GetKey()==EKeys::Escape||(Event.GetKey()==EKeys::Z&&(!Search||(!Search->HasKeyboardFocus()&&!Search->HasFocusedDescendants())))){if(!Event.IsRepeat())CloseGuide();return FReply::Handled();}
        if(Search&&(Search->HasKeyboardFocus()||Search->HasFocusedDescendants()))return Super::NativeOnPreviewKeyDown(Geometry,Event);
        if(Event.GetKey()==EKeys::Q){if(!Event.IsRepeat())HandleAction(0);return FReply::Handled();}
        if(Event.GetKey()==EKeys::E){if(!Event.IsRepeat())HandleAction(4);return FReply::Handled();}
    }
    return Super::NativeOnPreviewKeyDown(Geometry,Event);
}
void UDreamAugmentPanel::HandleAction(int32 Action)
{
    if(Action==3){CloseGuide();return;}
    if(Action==10){if(auto* P=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());P&&P->RerollAugmentChoices())RefreshChoice();return;}
    if(Action==0||Action==1||Action==4){Page=Action;Selection=-1;Filter=0;SearchText.Empty();if(Search)Search->SetText(FText::GetEmpty());}
    else if(Action>=20&&Action<=24){Filter=Action-20;Selection=-1;}
    else if(Action==30){if(auto* GI=GetGameInstance())if(auto* Pref=GI->GetSubsystem<UDreamPlayPreferences>()){Pref->TrackEvolution(static_cast<EAugmentID>(Selection));RefreshGuide();}return;}
    else if(Action==31){if(auto* GI=GetGameInstance())GI->GetSubsystem<UDreamPlayPreferences>()->ClearEvolutionGoal();RefreshGuide();return;}
    else if(Action>=1000&&Action<1256){Selection=Action-1000;RefreshDetail();return;}
    else return; // There is deliberately no evolve/confirm action in the catalog.
    RefreshGuide();
}
void UDreamAugmentPanel::RefreshChoice()
{
    auto* P=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());auto* Host=ReturnWidget.Get();if(!P || !P->DispatchTable || !Host) return;
    const auto Choices=P->GetCurrentAugmentChoices();const auto* D=P->DispatchTable.Get();
    if(auto* Title=Cast<UTextBlock>(Host->GetWidgetFromName(TEXT("Text_Title"))))Title->SetText(FText::FromString(TEXT("증강 선택")));
    Reroll->SetIsEnabled(P->CanRerollAugmentChoices());
    RerollLabel->SetText(FText::FromString(FString::Printf(TEXT("%d / 1"),P->GetAugmentRerollsRemaining())));
    for(int32 I=0;I<3;++I)
    {
        auto* Card=Cast<UButton>(Host->GetWidgetFromName(*FString::Printf(TEXT("Button_Choice%d"),I+1)));
        if(Card) Card->SetIsEnabled(Choices.IsValidIndex(I)&&P->IsAugmentChoiceInputReady());
    }
    const auto* Pref=GetGameInstance()?GetGameInstance()->GetSubsystem<UDreamPlayPreferences>():nullptr;
    const auto Goal=Pref?Pref->GetEvolutionGoal():static_cast<EAugmentID>(255);
    if(Choices==LastChoices&&LastGoal==int32(Goal)) return;LastChoices=Choices;LastGoal=int32(Goal);
    for(int32 I=0;I<3;++I)
    {
        if(!Choices.IsValidIndex(I)) continue;const auto ID=Choices[I];
        if(auto* Prop=FindFProperty<FByteProperty>(Host->GetClass(),*FString::Printf(TEXT("Choice%d"),I+1))) Prop->SetPropertyValue_InContainer(Host,uint8(ID));
        if(auto* Prop=FindFProperty<FEnumProperty>(Host->GetClass(),*FString::Printf(TEXT("Choice%d"),I+1)))
            Prop->GetUnderlyingProperty()->SetIntPropertyValue(Prop->ContainerPtrToValuePtr<void>(Host),int64(uint8(ID)));
        const auto* GoalDef=DreamAugmentCatalog::Find(Goal);
        const bool GoalMaterial=GoalDef&&(Goal==ID||GoalDef->Ingredients.Contains(ID));
        const bool Evolution=D->IsEvolution(ID);
        bool HasRoute=false;for(const auto& Def:DreamAugmentCatalog::All())if(Def.Evolution&&Def.Ingredients.Contains(ID)){HasRoute=true;break;}
        const bool Passive=!Evolution && D->GetAugmentCategory(ID)==EAugmentCategory::Passive;
        const FLinearColor Accent=Evolution?FLinearColor(1.f,.65f,.2f):HasRoute?FLinearColor(.7f,.5f,1.f):FLinearColor(1,1,1);
        if(auto* T=Cast<UTextBlock>(Host->GetWidgetFromName(*FString::Printf(TEXT("Text_Choice%dName"),I+1))))
        {
            T->SetText(FText::FromString((Evolution?TEXT("[진화] "):TEXT(""))+D->GetAugmentDisplayName(ID).ToString()));
            auto NameFont=T->GetFont();NameFont.Size=20;T->SetFont(NameFont);T->SetAutoWrapText(true);
            T->SetColorAndOpacity(Accent);
        }
        if(auto* Badge=Cast<UTextBlock>(Host->GetWidgetFromName(*FString::Printf(TEXT("DVI_ChoiceNumber%d"),I+1))))
        {
            Badge->SetText(FText::FromString(FString::Printf(TEXT("%02d · %s"),I+1,Evolution?TEXT("진화 카드"):HasRoute?(Passive?TEXT("패시브 · 진화 재료"):TEXT("액티브 · 진화 재료")):Passive?TEXT("패시브 / 중첩"):TEXT("액티브 / 자동"))));
            auto BadgeFont=Badge->GetFont();BadgeFont.Size=14;Badge->SetFont(BadgeFont);
            if(GoalMaterial)Badge->SetText(FText::FromString(FString(TEXT("★ 목표 · "))+(Evolution?TEXT("진화 카드"):Passive?TEXT("패시브 · 재료"):TEXT("액티브 · 재료"))));
            Badge->SetColorAndOpacity(GoalMaterial?FLinearColor(.3f,1,.8f):Accent);
        }
        if(auto* T=Cast<UTextBlock>(Host->GetWidgetFromName(*FString::Printf(TEXT("Text_Choice%dDesc"),I+1))))
        {
            const auto* Def=DreamAugmentCatalog::Find(ID);FString Description=Def?Def->CardText:D->GetCurrentAugmentDescription(ID).ToString();
            if(Passive)Description+=FString::Printf(TEXT("\n보유 %d중첩 → +1"),D->GetStackCount(ID));
            T->SetText(FText::FromString(Description));T->SetToolTipText(D->GetCurrentAugmentDescription(ID));
            auto Style=T->GetFont();Style.Size=16;T->SetFont(Style);
            T->SetAutoWrapText(true);T->SetLineHeightPercentage(1.1f);
            T->SetColorAndOpacity(FLinearColor(.96f,.95f,.99f));
            if(auto* TextSlot=Cast<UCanvasPanelSlot>(T->Slot))TextSlot->SetSize(FVector2D(TextSlot->GetSize().X,162));
        }
        if(auto* Card=Cast<UButton>(Host->GetWidgetFromName(*FString::Printf(TEXT("Button_Choice%d"),I+1))))Card->SetBackgroundColor(Accent);
        if(auto* Image=Cast<UImage>(Host->GetWidgetFromName(*FString::Printf(TEXT("Image_Choice%dIcon"),I+1))))
        {
            const auto* Def=DreamAugmentCatalog::Find(ID);const auto* Found=Icons?Icons->Icons.Find(ID):nullptr;
            if(!Found&&Icons&&Def)Found=Icons->Icons.Find(Def->Icon);
            Image->SetBrushFromTexture(Found?Found->Get():nullptr);Image->SetVisibility(Found?ESlateVisibility::HitTestInvisible:ESlateVisibility::Hidden);
        }
    }
}
void UDreamAugmentPanel::RefreshGuide()
{
    if(!Body||!DetailBody)return;Body->ClearChildren();DetailBody->ClearChildren();
    if(auto* ListScroll=Cast<UScrollBox>(Body->GetParent()))ListScroll->ScrollToStart();
    auto* P=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());auto* D=P?P->DispatchTable.Get():nullptr;if(!D)return;
    Header->SetText(FText::FromString(Page==4?TEXT("보유 중인 증강 · E"):Page==1?TEXT("진화 루트"):TEXT("증강 도감 · Q")));
    Summary=FString::Printf(TEXT("패시브 %d/6 · 액티브 %d/6 · 스킬 %d/6"),D->GetOwnedCount(EAugmentCategory::Passive),D->GetOwnedCount(EAugmentCategory::Active),D->GetSkillCount());if(auto* GI=GetGameInstance())Summary+=TEXT("\n")+GI->GetSubsystem<UDreamPlayPreferences>()->GetGoalText(D);
    Status->SetText(FText::FromString(Summary));
    if(Page==4)RefreshOwned();
    else for(const auto& Def:DreamAugmentCatalog::All())
    {
        if(Page==1&&!Def.Evolution)continue;if(Page==0&&Def.Evolution&&Filter!=4)continue;
        if(!MatchesFilter(Def.ID))continue;
        if(Selection<0||!MatchesFilter(static_cast<EAugmentID>(Selection)))Selection=int32(Def.ID);
        Button(Body,(Def.Evolution?TEXT("[진화] "):Def.Category==EAugmentCategory::Passive?TEXT("[패시브] "):TEXT("[액티브] "))+Def.Name,1000+int32(Def.ID));
    }
    if(Body->GetChildrenCount()==0){Selection=-1;Body->AddChildToVerticalBox(Label(TEXT("검색 조건에 맞는 증강이 없습니다."),17));}
    RefreshDetail();
}
void UDreamAugmentPanel::RefreshOwned()
{
    auto* P=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());if(!P||!P->DispatchTable)return;auto* D=P->DispatchTable.Get();TSet<EAugmentID> Seen;
    for(auto ID:D->GetAugmentHistory())if(!Seen.Contains(ID))
    {
        Seen.Add(ID);if(!MatchesFilter(ID))continue;const auto* Def=DreamAugmentCatalog::Find(ID);if(!Def)continue;
        if(Selection<0)Selection=int32(ID);
        Button(Body,FString::Printf(TEXT("[%s] %s × %d"),Def->Evolution?TEXT("진화"):Def->Category==EAugmentCategory::Passive?TEXT("패시브"):TEXT("액티브"),*Def->Name,D->GetStackCount(ID)),1000+int32(ID));
    }
    if(Seen.IsEmpty())Body->AddChildToVerticalBox(Label(TEXT("아직 획득한 증강이 없습니다."),18));
}
void UDreamAugmentPanel::RefreshDetail()
{
    if(!DetailBody)return;DetailBody->ClearChildren();
    if(auto* DetailsScroll=Cast<UScrollBox>(DetailBody->GetParent()))DetailsScroll->ScrollToStart();
    for(auto* Child:Body->GetAllChildren())if(auto* Size=Cast<USizeBox>(Child))if(auto* Item=Cast<UDreamAugmentActionButton>(Size->GetContent()))
    {
        FButtonStyle ItemStyle=Item->GetStyle();ItemStyle.Normal=FSlateColorBrush(Item->Action==Selection+1000?FLinearColor(.22f,.12f,.32f,.8f):FLinearColor::Transparent);Item->SetStyle(ItemStyle);
    }
    auto* P=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());auto* D=P?P->DispatchTable.Get():nullptr;
    const auto ID=static_cast<EAugmentID>(Selection);const auto* Def=Selection>=0?DreamAugmentCatalog::Find(ID):nullptr;
    if(!D||!Def){DetailBody->AddChildToVerticalBox(Label(TEXT("왼쪽 목록에서 증강을 선택하세요."),20));return;}
    if(const auto* Texture=Icons?Icons->Icons.Find(ID):nullptr)
    {
        auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(112);Size->SetHeightOverride(112);
        auto* Image=WidgetTree->ConstructWidget<UImage>();Image->SetBrushFromTexture(Texture->Get());Size->SetContent(Image);
        DetailBody->AddChildToVerticalBox(Size)->SetHorizontalAlignment(HAlign_Center);
    }
    DetailBody->AddChildToVerticalBox(Label(Def->Name,25))->SetPadding(FMargin(0,12));
    auto* Description=Label(D->GetCurrentAugmentDescription(ID).ToString(),18);
    Description->SetColorAndOpacity(FLinearColor(.96f,.95f,.99f));
    Description->SetLineHeightPercentage(1.1f);
    DetailBody->AddChildToVerticalBox(Description)->SetPadding(FMargin(0,0,0,16));
    if(Def->Evolution)
    {
        DetailBody->AddChildToVerticalBox(Label(D->GetEvolutionStatus(ID),18));
        Button(DetailBody,TEXT("이 진화를 목표로 추적"),30);Button(DetailBody,TEXT("진화 목표 해제"),31);
        for(auto Part:Def->Ingredients)Button(DetailBody,FString::Printf(TEXT("재료 · %s %d/1 → 상세"),*D->GetAugmentDisplayName(Part).ToString(),D->HasAcquiredAugment(Part)?1:0),1000+int32(Part));
    }
    else for(const auto& Route:DreamAugmentCatalog::All())if(Route.Evolution&&Route.Ingredients.Contains(ID))Button(DetailBody,TEXT("연결된 진화 → ")+Route.Name,1000+int32(Route.ID));
    DetailBody->AddChildToVerticalBox(Label(TEXT("조건 충족 후 진화 카드는 증강 선택에서 등장합니다."),14))->SetPadding(FMargin(0,12));
    // Explicit wrap width gives the scroll layout the correct paragraph height before painting.
    for(auto* Child:DetailBody->GetAllChildren())if(auto* Text=Cast<UTextBlock>(Child))Text->SetWrapTextAt(380.f);
}
void UDreamAugmentPanel::RefreshHUD()
{
    const float ViewScale=FMath::Max(.01f,UWidgetLayoutLibrary::GetViewportScale(this));
    const float Width=FMath::Clamp(UWidgetLayoutLibrary::GetViewportSize(this).X/ViewScale-40.f,150.f,396.f);
    const FVector2D Desired(Width,76.f*Width/396.f);
    if(!FVector2D(UGameViewportSubsystem::Get()->GetWidgetSlot(this).Offsets.Right,UGameViewportSubsystem::Get()->GetWidgetSlot(this).Offsets.Bottom).Equals(Desired,1.f))
    {
        SetDesiredSizeInViewport(Desired);SetAnchorsInViewport(FAnchors(0.f,1.f));
    }
    auto* P=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());auto* PC=Cast<AMainPlayerController>(GetOwningPlayer());auto* GI=GetGameInstance<UDreamVeilGameInstance>();
    const bool Visible=P && PC && GI && (GI->IsInLevelMap() || GI->IsInEndless()) && P->CombatStats && !P->CombatStats->IsDead() && !PC->IsMenuWidgetOpen() && !P->HasAugmentChoices();
    float Opacity=PC?FMath::Clamp(PC->SkillHUDOpacity,0.f,1.f):0.f;
    if(PC&&PC->bFadeSkillHUDWithCorruption)if(auto* GM=GetWorld()->GetAuthGameMode<AMainGameModeBase>())
    {
        const float Progress=FMath::Max(GM->GetLevelTimeProgress(),GM->GetCorruption());
        const float Fade=FMath::Clamp((Progress-PC->SkillHUDFadeStart)/FMath::Max(.01f,1.f-PC->SkillHUDFadeStart),0.f,1.f);
        Opacity*=FMath::Lerp(1.f,FMath::Clamp(PC->SkillHUDMinOpacity,0.f,1.f),Fade);
    }
    SetRenderOpacity(Visible&&PC->bShowEvolvedSkillHUD?Opacity:0.f);if(!Visible || !P->DispatchTable) return;
    auto* D=P->DispatchTable.Get();
    const double Now=GetWorld()?GetWorld()->GetRealTimeSeconds():0;
    const bool DescriptionChanged=LastHUDPawn.Get()!=P||LastHUDRevision!=D->GetAugmentRevision()||Now>=NextHUDDescriptionRefresh;
    if(DescriptionChanged){LastHUDPawn=P;LastHUDRevision=D->GetAugmentRevision();NextHUDDescriptionRefresh=Now+1;}

    auto* Feedback=GetWorld()->GetSubsystem<UDreamAugmentFeedback>();
    if(FeedbackLabel){const FString Message=Feedback?Feedback->Notice(P):FString();if(FeedbackLabel->GetText().ToString()!=Message)FeedbackLabel->SetText(FText::FromString(Message));FeedbackLabel->SetVisibility(Message.IsEmpty()?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible);}
    const bool LastBreath=D->HasAcquiredAugment(EAugmentID::LastBreath);
    if(LastBreath && !HealthCapMarker)
    {
        TArray<UUserWidget*> Screens;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,Screens,UUserWidget::StaticClass(),true);
        for(auto* Screen:Screens)if(auto* Bar=Screen->GetWidgetFromName(TEXT("Bar_Health")))
        {
            auto* Canvas=Cast<UCanvasPanel>(Bar->GetParent());auto* BarSlot=Cast<UCanvasPanelSlot>(Bar->Slot);
            if(Canvas && BarSlot)
            {
                HealthCapMarker=Screen->WidgetTree->ConstructWidget<UBorder>();HealthCapMarker->SetBrushColor(FLinearColor(1.f,.75f,.3f));
                HealthCapMarker->SetToolTipText(FText::FromString(TEXT("마지막 한숨 · 현재 체력 / 회복 상한 20%")));
                auto* MarkerSlot=Canvas->AddChildToCanvas(HealthCapMarker);MarkerSlot->SetAnchors(BarSlot->GetAnchors());MarkerSlot->SetAlignment(BarSlot->GetAlignment());
                MarkerSlot->SetPosition(BarSlot->GetPosition()+FVector2D(BarSlot->GetSize().X*P->CombatStats->GetHealthCapRatio(),-2));
                MarkerSlot->SetSize(FVector2D(2,BarSlot->GetSize().Y+4));MarkerSlot->SetZOrder(50);HealthCapMarker->SetVisibility(ESlateVisibility::HitTestInvisible);break;
            }
        }
    }
    if(HealthCapMarker)HealthCapMarker->SetVisibility(LastBreath?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    for(int32 I=0;I<6;++I)
    {
        const auto ID=D->GetSkillAtSlot(I);const auto* Def=DreamAugmentCatalog::Find(ID);const bool Owned=Def!=nullptr;
        const float Cooldown=D->GetSkillCooldownRemaining(I);
        if(SkillLabels.IsValidIndex(I))
        {
            const bool Cooling=Owned&&Cooldown>0.f;
            const int32 Seconds=Cooling?FMath::Max(1,FMath::CeilToInt(Cooldown)):0;
            if(DisplayedCooldownSeconds[I]!=Seconds)
            {SkillLabels[I]->SetText(Cooling?FText::AsNumber(Seconds):FText::GetEmpty());DisplayedCooldownSeconds[I]=Seconds;}
            SkillLabels[I]->SetVisibility(Cooling?ESlateVisibility::HitTestInvisible:ESlateVisibility::Hidden);
            const auto* Texture=Owned&&Icons?Icons->Icons.Find(ID):nullptr;
            if(!Texture&&Owned&&Icons)Texture=Icons->Icons.Find(Def->Icon);
            if(SkillIcons[I]->GetBrush().GetResourceObject()!=(Texture?Texture->Get():nullptr))SkillIcons[I]->SetBrushFromTexture(Texture?Texture->Get():nullptr);
            SkillIcons[I]->SetVisibility(Texture?ESlateVisibility::HitTestInvisible:ESlateVisibility::Hidden);
            SkillIcons[I]->SetColorAndOpacity(Cooling?FLinearColor(.38f,.38f,.42f,.9f):FLinearColor::White);
            const auto* Runtime=D->FindRuntime();const bool Active=Runtime&&Runtime->HasFeedbackState(ID);
            const bool Failed=Feedback&&Feedback->FailureUntil(ID)>GetWorld()->GetTimeSeconds();
            const bool Affordable=Owned&&P->GetCurrentStamina()>=D->GetSkillCost(I)+20;
            const float Pulse=.8f+.2f*FMath::Sin(GetWorld()->GetTimeSeconds()*5);
            SkillFrames[I]->SetBrushColor(Failed?FLinearColor(1,.12f,.2f):Active?FLinearColor(.7f,.42f,1,1)*Pulse:!Cooling&&Affordable?FLinearColor(.8f,.55f,1):FLinearColor(.35f,.2f,.45f));
            if(Active)SkillIcons[I]->SetColorAndOpacity(FLinearColor(1,.85f,1));
            else if(!Cooling&&!Affordable)SkillIcons[I]->SetColorAndOpacity(FLinearColor(.5f,.4f,.55f));
            SkillFrames[I]->SetRenderOpacity(Owned?1.f:0.f);
            if(DescriptionChanged)SkillIcons[I]->SetToolTipText(Owned?D->GetCurrentAugmentDescription(ID):FText::FromString(TEXT("진화 스킬 미해금")));
        }
    }


}
FString UDreamAugmentPanel::GetSummary() const
{
    if(Mode!=EDreamAugmentPanelMode::SkillHUD)return Summary;
    const auto* P=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());
    const auto* PC=Cast<AMainPlayerController>(GetOwningPlayer());
    if(!P||!P->DispatchTable||!PC)return FString();
    const auto* D=P->DispatchTable.Get();FString Result;
    for(int32 I=0;I<6;++I)
    {
        const auto* Definition=DreamAugmentCatalog::Find(D->GetSkillAtSlot(I));
        Result+=FString::Printf(TEXT("[%s] %s\n%s"),*PC->GetAugmentSkillKeyText(I),Definition?*Definition->Name:TEXT("빈 칸"),Definition?*D->GetSkillStateText(I):TEXT("미해금"));
    }
    return Result;
}




#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
class SDreamRerollGlyph : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SDreamRerollGlyph){}SLATE_END_ARGS()
    void Construct(const FArguments&){}
    virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(46,46);}
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& Geometry,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled)const override
    {
        const FVector2D Size=Geometry.GetLocalSize(),Center=Size*.5;const float R=FMath::Min(Size.X,Size.Y)*.32f;
        const FLinearColor Color=FLinearColor(.95f,.85f,.62f)*(Enabled?1.f:.4f)*Style.GetColorAndOpacityTint();
        for(int32 Half=0;Half<2;++Half)
        {
            TArray<FVector2D> Arc;for(int32 I=0;I<=22;++I){const float A=FMath::DegreesToRadians(Half*180.f+25.f+I*130.f/22);Arc.Add(Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*R);}
            FSlateDrawElement::MakeLines(Out,Layer,Geometry.ToPaintGeometry(),Arc,ESlateDrawEffect::None,Color,true,4);
            const FVector2D Tip=Arc.Last(),Dir=(Tip-Arc[Arc.Num()-2]).GetSafeNormal(),Side(-Dir.Y,Dir.X);
            TArray<FVector2D> Head{Tip-Dir*8+Side*6,Tip,Tip-Dir*8-Side*6};FSlateDrawElement::MakeLines(Out,Layer+1,Geometry.ToPaintGeometry(),Head,ESlateDrawEffect::None,Color,true,5);
        }return Layer+2;
    }
};
TSharedRef<SWidget> UDreamRerollGlyph::RebuildWidget(){return SNew(SDreamRerollGlyph);}

FReply UDreamAugmentChoiceHost::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    const auto Key=Event.GetKey();
    if(Key==EKeys::Q||Key==EKeys::E)
    {
        if(!Event.IsRepeat())
        {
            auto* P=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());
            if(P&&P->HasAugmentChoices()&&P->CombatStats&&!P->CombatStats->IsDead())
            {
                if(!Overlay.IsValid()||!Overlay->IsInViewport())Overlay=UDreamAugmentPanel::OpenGuide(GetOwningPlayer(),this);
                if(Overlay.IsValid())Overlay->HandleAction(Key==EKeys::E?4:0);
            }
        }
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry,Event);
}
void UDreamAugmentChoiceHost::NativeDestruct()
{
    if(Overlay.IsValid())Overlay->RemoveFromParent();Overlay.Reset();Super::NativeDestruct();
}

void UDreamAugmentPanel::SearchChanged(const FText& Text){SearchText=Text.ToString();Selection=-1;RefreshGuide();}
bool UDreamAugmentPanel::MatchesFilter(EAugmentID ID)const
{
    const auto* Def=DreamAugmentCatalog::Find(ID);if(!Def||(!SearchText.IsEmpty()&&!Def->Name.Contains(SearchText)))return false;
    const auto* P=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());auto* D=P?P->DispatchTable.Get():nullptr;
    if(Filter==1)return !Def->Evolution&&Def->Category==EAugmentCategory::Passive;
    if(Filter==2)return !Def->Evolution&&Def->Category==EAugmentCategory::Active;
    if(Filter==3)return D&&D->HasAcquiredAugment(ID);
    if(Filter==4)return D&&Def->Evolution&&D->CanEvolve(ID);
    return true;
}
