#include "DreamBossHealthWidget.h"
#include "BossMonster.h"
#include "MainGameModeBase.h"
#include "CombatStatsComponent.h"
#include "DreamVeilGameInstance.h"
#include "DreamObjectPoolSubsystem.h"
#include "MainPlayerController.h"
#include "MainPlayerCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
UDreamBossHealthWidget::UDreamBossHealthWidget(const FObjectInitializer& I):Super(I)
{
    static ConstructorHelpers::FObjectFinder<UFont> F(TEXT("/Game/UI/Fonts/Pretendard-SemiBold_Font"));Font=F.Object;
    static ConstructorHelpers::FObjectFinder<UTexture2D> T(TEXT("/Game/UI/DreamVeilSkin/Frames/T_Frame_Panel_9Slice"));Frame=T.Object;
}
void UDreamBossHealthWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Canvas;
    Panel=WidgetTree->ConstructWidget<UBorder>();Panel->SetPadding(FMargin(18,9));
    FSlateBrush B;B.SetResourceObject(Frame);B.DrawAs=ESlateBrushDrawType::Box;B.Margin=FMargin(.0625f);B.ImageSize=FVector2D(128);Panel->SetBrush(B);
    auto* BarSlot=Canvas->AddChildToCanvas(Panel);BarSlot->SetAnchors(FAnchors(.30f,1.f,.70f,1.f));BarSlot->SetOffsets(FMargin(0,-86,0,44));
    auto* Fill=WidgetTree->ConstructWidget<UBorder>();Fill->SetBrushColor(FLinearColor(.025f,.012f,.04f,.88f));Fill->SetPadding(FMargin(8,2));Panel->SetContent(Fill);
    auto* Rows=WidgetTree->ConstructWidget<UVerticalBox>();Fill->SetContent(Rows);
    Announcement=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("BossDefeatedAnnouncement"));
    Announcement->SetFont(FSlateFontInfo(Font.Get(),36));Announcement->SetJustification(ETextJustify::Center);
    Announcement->SetText(FText::FromString(TEXT("보스 처치")));Announcement->SetColorAndOpacity(FLinearColor(1.f,.85f,.5f));
    Announcement->SetShadowColorAndOpacity(FLinearColor(0,0,0,.9f));Announcement->SetShadowOffset(FVector2D(2,2));
    auto* NoticeSlot=Canvas->AddChildToCanvas(Announcement);NoticeSlot->SetAnchors(FAnchors(.5f,.5f));
    NoticeSlot->SetAlignment(FVector2D(.5f,.5f));NoticeSlot->SetAutoSize(true);NoticeSlot->SetPosition(FVector2D::ZeroVector);
    Announcement->SetVisibility(ESlateVisibility::Collapsed);
    auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetHeightOverride(17);Rows->AddChildToVerticalBox(Size);
    Health=WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),TEXT("BossHealthBar"));Health->SetFillColorAndOpacity(FLinearColor(.65f,.035f,.15f));Size->SetContent(Health);
    SetVisibility(ESlateVisibility::HitTestInvisible);Panel->SetVisibility(ESlateVisibility::Collapsed);RefreshBoss();
}
void UDreamBossHealthWidget::NativeConstruct()
{Super::NativeConstruct();NextSearch=0;RefreshBoss();if(GetWorld())GetWorld()->GetTimerManager().SetTimer(RefreshTimer,this,&UDreamBossHealthWidget::RefreshBoss,.1f,true);}
void UDreamBossHealthWidget::NativeDestruct()
{if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);Boss.Reset();Super::NativeDestruct();}
void UDreamBossHealthWidget::NativeTick(const FGeometry& Geometry,float DeltaSeconds)
{Super::NativeTick(Geometry,DeltaSeconds);if(UGameplayStatics::IsGamePaused(this))RefreshBoss();}
ABossMonster* UDreamBossHealthWidget::GetDisplayedBoss() const
{
    auto* GI=GetWorld()?GetWorld()->GetGameInstance<UDreamVeilGameInstance>():nullptr;auto* B=Boss.Get();
    return GI&&GI->IsInLevelMap()&&!GI->IsInEndless()&&B&&!B->IsHidden()&&BossGeneration==UDreamObjectPoolSubsystem::Generation(B)&&B->MonsterCombatStats&&!B->MonsterCombatStats->IsDead()?B:nullptr;
}
float UDreamBossHealthWidget::GetDisplayedHealthPercent() const
{auto* B=GetDisplayedBoss();return B?B->MonsterCombatStats->GetHealthPercentage():0.f;}
void UDreamBossHealthWidget::RefreshBoss()
{
    if(!Panel)return;auto* GI=GetWorld()?GetWorld()->GetGameInstance<UDreamVeilGameInstance>():nullptr;
    if(!GI||!GI->IsInLevelMap()||GI->IsInEndless()){Boss.Reset();Panel->SetVisibility(ESlateVisibility::Collapsed);Announcement->SetVisibility(ESlateVisibility::Collapsed);return;}
    const float Time=GetWorld()->GetTimeSeconds();
    if(!GetDisplayedBoss()&&Time>=NextSearch)
    {
        NextSearch=Time+1.f;Boss.Reset();
        for(TActorIterator<ABossMonster> It(GetWorld());It;++It)
            if(!It->IsHidden()&&It->MonsterCombatStats&&!It->MonsterCombatStats->IsDead()){Boss=*It;BossGeneration=UDreamObjectPoolSubsystem::Generation(*It);break;}
    }
    auto* B=GetDisplayedBoss();auto* PC=Cast<AMainPlayerController>(GetOwningPlayer());auto* P=PC?Cast<AMainPlayerCharacter>(PC->GetPawn()):nullptr;
    const bool Visible=B&&P&&P->CombatStats&&!P->CombatStats->IsDead()&&!PC->IsMenuWidgetOpen()&&!P->HasAugmentChoices();
    Panel->SetVisibility(Visible?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    const auto* Mode=GetWorld()->GetAuthGameMode<AMainGameModeBase>();
    const bool ShowNotice=Mode&&Mode->IsBossDefeatPresentation()&&P&&P->CombatStats&&!P->CombatStats->IsDead()&&!PC->IsMenuWidgetOpen();
    Announcement->SetVisibility(ShowNotice?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    if(B)Health->SetPercent(GetDisplayedHealthPercent());
}
