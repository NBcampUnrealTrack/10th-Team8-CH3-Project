#include "DreamPauseWidget.h"
#include "DreamPlayPolish.h"
#include "Engine/GameInstance.h"
#include "DreamUIBlueprintLibrary.h"
#include "DreamAugmentPanel.h"
#include "DispatchTableComponent.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/Image.h"
#include "MainPlayerController.h"
#include "MainPlayerCharacter.h"
#include "CombatStatsComponent.h"
#include "WeaponBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Engine/Texture2D.h"
#include "Engine/Font.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Styling/CoreStyle.h"
#include "UObject/ConstructorHelpers.h"

UDreamPauseWidget::UDreamPauseWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
    SetIsFocusable(true);
    static ConstructorHelpers::FObjectFinder<UTexture2D> Panel(TEXT("/Game/UI/DreamVeilSkin/Frames/T_Frame_Panel_9Slice"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> Normal(TEXT("/Game/UI/DreamVeilSkin/Buttons/T_Button_Normal"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> Hover(TEXT("/Game/UI/DreamVeilSkin/Buttons/T_Button_Hovered"));
    static ConstructorHelpers::FObjectFinder<UTexture2D> Press(TEXT("/Game/UI/DreamVeilSkin/Buttons/T_Button_Pressed"));
    static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("/Game/UI/Fonts/Pretendard-SemiBold_Font"));
    static ConstructorHelpers::FObjectFinder<UDreamAugmentIconSet> Icons(TEXT("/Game/UI/AugmentIcons/DA_AugmentIcons"));
    AugmentIcons=Icons.Object;
    PanelTexture=Panel.Object; NormalTexture=Normal.Object; HoverTexture=Hover.Object;
    PressTexture=Press.Object; MenuFont=Font.Object;
}

void UDreamPauseWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!WidgetTree || WidgetTree->RootWidget) return;
    auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget=Canvas;
    auto* Shade=WidgetTree->ConstructWidget<UBorder>();
    Shade->SetBrushColor(FLinearColor(0,0,0,.65f));
    auto* ShadeSlot=Canvas->AddChildToCanvas(Shade);
    ShadeSlot->SetAnchors(FAnchors(0,0,1,1)); ShadeSlot->SetOffsets(FMargin(0));
    auto* Scale=WidgetTree->ConstructWidget<UScaleBox>();Scale->SetStretch(EStretch::ScaleToFit);auto* ScaleSlot=Canvas->AddChildToCanvas(Scale);ScaleSlot->SetAnchors(FAnchors(0,0,1,1));ScaleSlot->SetOffsets(FMargin(24));
    auto* Fixed=WidgetTree->ConstructWidget<USizeBox>();Fixed->SetWidthOverride(944);Fixed->SetHeightOverride(560);Scale->SetContent(Fixed);
    auto* PanelFill=WidgetTree->ConstructWidget<UBorder>();PanelFill->SetBrushColor(FLinearColor(.025f,.02f,.035f,.96f));Fixed->SetContent(PanelFill);
    auto* Panel=WidgetTree->ConstructWidget<UBorder>();FSlateBrush Frame;Frame.SetResourceObject(PanelTexture);Frame.DrawAs=ESlateBrushDrawType::Box;Frame.Margin=FMargin(.0625f);Panel->SetBrush(Frame);Panel->SetPadding(FMargin(32));PanelFill->SetContent(Panel);
    auto* Columns=WidgetTree->ConstructWidget<UHorizontalBox>(); Panel->SetContent(Columns);
    auto* Left=WidgetTree->ConstructWidget<USizeBox>(); Left->SetWidthOverride(400);
    Columns->AddChildToHorizontalBox(Left)->SetPadding(FMargin(0,0,24,0));
    auto* Rows=WidgetTree->ConstructWidget<UVerticalBox>(); Left->SetContent(Rows);
    auto AddLabel=[this](const TCHAR* Caption,int32 Size)
    {
        auto* Label=WidgetTree->ConstructWidget<UTextBlock>();
        Label->SetText(FText::FromString(Caption)); Label->SetJustification(ETextJustify::Center);
        Label->SetFont(MenuFont?FSlateFontInfo(MenuFont.Get(),Size):FCoreStyle::GetDefaultFontStyle("Regular",Size));
        Label->SetColorAndOpacity(FLinearColor(.88f,.86f,.80f,1)); return Label;
    };
    Rows->AddChildToVerticalBox(AddLabel(TEXT("일시정지"),32))->SetPadding(FMargin(0,4,0,18));
    StatText=AddLabel(TEXT(""),19); StatText->SetJustification(ETextJustify::Left);
    Rows->AddChildToVerticalBox(StatText)->SetPadding(FMargin(12,0,12,18));
    auto Brush=[](UTexture2D* Texture)
    {
        FSlateBrush B; B.SetResourceObject(Texture); B.DrawAs=ESlateBrushDrawType::Box; B.Margin=FMargin(.12f); return B;
    };
    FButtonStyle Style; Style.Normal=Brush(NormalTexture); Style.Hovered=Brush(HoverTexture); Style.Pressed=Brush(PressTexture);
    auto AddButton=[this,Rows,&Style,&AddLabel](const TCHAR* Name,const TCHAR* Caption)
    {
        auto* Size=WidgetTree->ConstructWidget<USizeBox>(); Size->SetHeightOverride(52);
        Rows->AddChildToVerticalBox(Size)->SetPadding(FMargin(0,6));
        auto* Button=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),Name);
        Button->SetStyle(Style); Button->SetContent(AddLabel(Caption,20)); Size->SetContent(Button); return Button;
    };
    AddButton(TEXT("Pause_Resume"),TEXT("게임으로 돌아가기"))->OnClicked.AddUniqueDynamic(this,&UDreamPauseWidget::ResumeGame);
    Rows->AddChildToVerticalBox(AddLabel(TEXT("증강 도감 Q  /  보유 증강 E"),18))->SetPadding(FMargin(0,10));
    AddButton(TEXT("Pause_Quit"),TEXT("게임 종료"))->OnClicked.AddUniqueDynamic(this,&UDreamPauseWidget::QuitGame);
    Rows->AddChildToVerticalBox(AddLabel(TEXT("ESC / Z · 게임으로 돌아가기"),16))->SetPadding(FMargin(0,18,0,0));
    auto* Right=WidgetTree->ConstructWidget<USizeBox>(); Right->SetWidthOverride(456);
    Columns->AddChildToHorizontalBox(Right);
    auto* AugmentRows=WidgetTree->ConstructWidget<UVerticalBox>(); Right->SetContent(AugmentRows);
    AugmentRows->AddChildToVerticalBox(AddLabel(TEXT("설정 · 전투 기록"),28))->SetPadding(FMargin(0,4,0,18));
    AugmentScroll=WidgetTree->ConstructWidget<UScrollBox>();AugmentScroll->SetAlwaysShowScrollbar(true);
    auto* ListSize=WidgetTree->ConstructWidget<USizeBox>(); ListSize->SetHeightOverride(416); ListSize->SetContent(AugmentScroll);
    AugmentRows->AddChildToVerticalBox(ListSize);

}
void UDreamPauseWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (StatText)
    {
        const auto* PawnCharacter=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());
        if(PawnCharacter && PawnCharacter->CombatStats)
        {
            const auto* Stats=PawnCharacter->CombatStats.Get();
            const auto* Weapon=PawnCharacter->GetCurrentWeapon();
            const FString Info=FString::Printf(TEXT("레벨  %d\n체력  %.0f / %.0f\n공격력  %.2f     방어력  %.2f\n이동 속도  %.1f m/s\n무기 피해량  %.2f     초당 발사  %.2f"),
                PawnCharacter->GetPlayerLevel(),Stats->GetCurrentHealth(),Stats->GetMaxHealth(),Stats->GetAttackPower(),Stats->GetDefencePower(),
                PawnCharacter->GetCharacterMovement()->MaxWalkSpeed/100.0f,
                Weapon?Weapon->GetFinalDamage():0.0f,Weapon?1.0f/FMath::Max(.01f,Weapon->GetFireInterval()):0.0f);
            StatText->SetText(FText::FromString(Info));
        }
        else StatText->SetText(FText::GetEmpty());
    }
    if(AugmentScroll)
    {
        AugmentScroll->ClearChildren();auto* Note=WidgetTree->ConstructWidget<UTextBlock>();
        Note->SetFont(FSlateFontInfo(MenuFont.Get(),18));Note->SetAutoWrapText(true);
        Note->SetText(FText::FromString(TEXT("증강 도감: Q\n보유 중인 증강: E\n스킬: 1 ~ 6\n\n도움말은 메인 메뉴 Help → 조작에서 확인하세요.")));AugmentScroll->AddChild(Note);
    }
    if(AugmentScroll)
    {
        auto* Pref=GetGameInstance()?GetGameInstance()->GetSubsystem<UDreamPlayPreferences>():nullptr;
        if(Pref)
        {
            auto* Button=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("SimplifiedEffectsToggle"));
            FButtonStyle Style;FSlateBrush Brush;Brush.SetResourceObject(NormalTexture);Brush.DrawAs=ESlateBrushDrawType::Box;Brush.Margin=FMargin(.12f);Style.Normal=Style.Hovered=Style.Pressed=Brush;Button->SetStyle(Style);
            EffectModeLabel=WidgetTree->ConstructWidget<UTextBlock>();EffectModeLabel->SetFont(FSlateFontInfo(MenuFont.Get(),20));EffectModeLabel->SetText(FText::FromString(Pref->IsSimplifiedEffects()?TEXT("증강 연출 · 간소화"):TEXT("증강 연출 · 기본")));EffectModeLabel->SetJustification(ETextJustify::Center);EffectModeLabel->SetAutoWrapText(false);Button->SetToolTipText(FText::FromString(TEXT("클릭하여 기본 / 간소화 전환")));Button->SetContent(EffectModeLabel);auto* ActionSize=WidgetTree->ConstructWidget<USizeBox>();ActionSize->SetHeightOverride(48);ActionSize->SetContent(Button);Button->OnClicked.AddUniqueDynamic(this,&UDreamPauseWidget::ToggleEffectMode);AugmentScroll->AddChild(ActionSize);
            auto* P=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());auto* Details=WidgetTree->ConstructWidget<UTextBlock>();Details->SetFont(FSlateFontInfo(MenuFont.Get(),16));Details->SetAutoWrapText(true);Details->SetColorAndOpacity(FLinearColor(.88f,.8f,.96f));
            Details->SetText(FText::FromString(TEXT("\n간소화에서도 장판·방어벽·적 공격 경고는 유지됩니다.\n\n")+Pref->GetGoalText(P?P->DispatchTable.Get():nullptr)+TEXT("\n\n최근 전투\n")+Pref->GetLastReportText()));AugmentScroll->AddChild(Details);
        }
    }
    SetKeyboardFocus();
}
FReply UDreamPauseWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if ((Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::Z))
    {
        if (!Event.IsRepeat()) ResumeGame();
        return FReply::Handled();
    }
    return Super::NativeOnPreviewKeyDown(Geometry,Event);
}
FReply UDreamPauseWidget::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if ((Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::Z))
    {
        if (!Event.IsRepeat()) ResumeGame();
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(Geometry,Event);
}
void UDreamPauseWidget::ResumeGame()
{
    if (auto* PC=Cast<AMainPlayerController>(GetOwningPlayer())) PC->TogglePauseMenu();
}
void UDreamPauseWidget::QuitGame()
{
    if (auto* PC=GetOwningPlayer()) UKismetSystemLibrary::QuitGame(this,PC,EQuitPreference::Quit,false);
}

FText UDreamPauseWidget::GetDisplayText() const
{
    return StatText?StatText->GetText():FText::GetEmpty();
}

void UDreamPauseWidget::RefreshAugmentList()
{
    AugmentSummary.Empty();
    if (!AugmentScroll) return;
    AugmentScroll->ClearChildren();
    const auto* Pawn=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());
    TArray<EAugmentID> Unique;
    TMap<EAugmentID,int32> Counts;
    if (Pawn && Pawn->DispatchTable)
        for (EAugmentID ID:Pawn->DispatchTable->GetAugmentHistory())
        { if (!Counts.Contains(ID)) Unique.Add(ID); Counts.FindOrAdd(ID)++; }
    auto Label=[this](const FString& Text,int32 Size,FLinearColor Color)
    {
        auto* L=WidgetTree->ConstructWidget<UTextBlock>();L->SetText(FText::FromString(Text));
        L->SetFont(MenuFont?FSlateFontInfo(MenuFont.Get(),Size):FCoreStyle::GetDefaultFontStyle("Regular",Size));
        L->SetColorAndOpacity(Color);L->SetAutoWrapText(true);L->SetWrapTextAt(376.f);return L;
    };
    if (Unique.IsEmpty())
    { AugmentSummary=TEXT("아직 획득한 증강이 없습니다.");if (!EmptyAugmentLabel) EmptyAugmentLabel=Label(AugmentSummary,18,FLinearColor(.75f,.72f,.8f));AugmentScroll->AddChild(EmptyAugmentLabel); return; }
    for (EAugmentID ID:Unique)
    {
        FString Name=UDispatchTableComponent::IsEvolution(ID)?TEXT("[진화] "):
            UDispatchTableComponent::GetAugmentCategory(ID)==EAugmentCategory::Passive?TEXT("[패시브] "):TEXT("[액티브] ");
        Name+=UDispatchTableComponent::GetAugmentDisplayName(ID).ToString();
        if (Counts[ID]>1) Name+=FString::Printf(TEXT(" × %d"),Counts[ID]);
        FString State;
        if (Pawn && Pawn->CombatStats)
        {
            const float Ratio=Pawn->CombatStats->GetCurrentHealth()/FMath::Max(1.f,Pawn->CombatStats->GetMaxHealth());
            if (ID==EAugmentID::Berserker)
            {
                const auto* D=Pawn->DispatchTable.Get();const bool Evolved=D && D->HasAcquiredAugment(EAugmentID::LastBreath);
                const float Mult=Ratio<=.1f?(Evolved?D->LastBreathCriticalMultiplier:BERSERKER_CRITICAL_MULTIPLIER):Ratio<=.4f?(Evolved?D->LastBreathAttackMultiplier:BERSERKER_MULTIPLIER):1.f;
                State=FString::Printf(TEXT("현재 공격력 %.1f배"),Mult);
            }
            if (ID==EAugmentID::LastFortress) State=Ratio<=LAST_FORTRESS_THRESHOLD?TEXT("현재 활성"):TEXT("현재 비활성");
        }
        if (!State.IsEmpty()) Name+=TEXT(" · ")+State;
        const FString Description=Pawn && Pawn->DispatchTable?Pawn->DispatchTable->GetCurrentAugmentDescription(ID).ToString():UDispatchTableComponent::GetAugmentDescription(ID).ToString();
        AugmentSummary+=Name+TEXT("\n")+Description+TEXT("\n");
        auto* Box=CachedAugmentRows.FindRef(ID).Get();
        if (!Box)
        {
            Box=WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),*FString::Printf(TEXT("AugmentRow_%d"),static_cast<int32>(ID)));
            CachedAugmentRows.Add(ID,Box);
            if (AugmentIcons)
                if (const auto* Texture=AugmentIcons->Icons.Find(ID))
                {
                    auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(44);Size->SetHeightOverride(44);
                    auto* Icon=WidgetTree->ConstructWidget<UImage>();Icon->SetBrushFromTexture(Texture->Get(),true);Size->SetContent(Icon);
                    Box->AddChildToHorizontalBox(Size)->SetPadding(FMargin(0,2,12,0));
                }
            auto* TextRows=WidgetTree->ConstructWidget<UVerticalBox>();
            Box->AddChildToHorizontalBox(TextRows)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            TextRows->AddChildToVerticalBox(Label(TEXT(""),18,FLinearColor(.93f,.86f,.67f)))->SetPadding(FMargin(0,0,0,4));
            TextRows->AddChildToVerticalBox(Label(TEXT(""),14,FLinearColor(.8f,.79f,.84f)));
        }
        auto* TextRows=CastChecked<UVerticalBox>(Box->GetChildAt(Box->GetChildrenCount()-1));
        CastChecked<UTextBlock>(TextRows->GetChildAt(0))->SetText(FText::FromString(Name));
        CastChecked<UTextBlock>(TextRows->GetChildAt(1))->SetText(FText::FromString(Description));
        Cast<UScrollBoxSlot>(AugmentScroll->AddChild(Box))->SetPadding(FMargin(0,0,12,14));
    }
}

void UDreamPauseWidget::OpenAugmentGuide()
{
    UDreamAugmentPanel::OpenGuide(GetOwningPlayer());
}

void UDreamPauseWidget::ToggleEffectMode()
{
    if(auto* GI=GetGameInstance())if(auto* Pref=GI->GetSubsystem<UDreamPlayPreferences>())
    {Pref->SetSimplifiedEffects(!Pref->IsSimplifiedEffects());if(EffectModeLabel)EffectModeLabel->SetText(FText::FromString(Pref->IsSimplifiedEffects()?TEXT("증강 연출 · 간소화"):TEXT("증강 연출 · 기본")));}
}
