#include "DreamTimeWarningWidget.h"
#include "MainGameModeBase.h"
#include "DreamVeilGameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Styling/CoreStyle.h"

UDreamTimeWarningWidget::UDreamTimeWarningWidget(const FObjectInitializer& Initializer):Super(Initializer)
{
    static ConstructorHelpers::FObjectFinder<UFont> Font(TEXT("/Game/UI/Fonts/Pretendard-SemiBold_Font"));
    static ConstructorHelpers::FObjectFinder<USoundBase> Sound(TEXT("/Game/Audio/SFX/S_TimeWarning"));
    WarningFont=Font.Object;WarningSound=Sound.Object;
}
void UDreamTimeWarningWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!WidgetTree || WidgetTree->RootWidget)return;
    auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Canvas;
    WarningText=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("TimeWarning_Text"));
    WarningText->SetJustification(ETextJustify::Center);
    WarningText->SetFont(WarningFont?FSlateFontInfo(WarningFont.Get(),22):FCoreStyle::GetDefaultFontStyle("Regular",22));
    WarningText->SetShadowColorAndOpacity(FLinearColor::Black);WarningText->SetShadowOffset(FVector2D(1,1));
    auto* WarningSlot=Canvas->AddChildToCanvas(WarningText);
    WarningSlot->SetAnchors(FAnchors(.5f,.18f));WarningSlot->SetAlignment(FVector2D(.5f,0));WarningSlot->SetSize(FVector2D(600,48));
}
void UDreamTimeWarningWidget::NativeConstruct()
{
    Super::NativeConstruct();SetVisibility(ESlateVisibility::HitTestInvisible);
    bWarned30=bWarned10=false;LastSecond=INDEX_NONE;
    UpdateWarning();
    if(GetWorld())GetWorld()->GetTimerManager().SetTimer(RefreshTimer,this,&UDreamTimeWarningWidget::UpdateWarning,.25f,true);
}
void UDreamTimeWarningWidget::NativeDestruct()
{
    if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
    Super::NativeDestruct();
}
void UDreamTimeWarningWidget::UpdateWarning()
{
    const auto* GI=GetGameInstance<UDreamVeilGameInstance>();
    const auto* Mode=Cast<AMainGameModeBase>(UGameplayStatics::GetGameMode(this));
    const int32 Seconds=Mode?FMath::CeilToInt(Mode->GetLevelTimeRemaining()):0;
    if(!WarningText)return;
    if(!GI || !GI->IsInLevelMap() || Seconds<=0 || Seconds>30)
    {
        WarningText->SetVisibility(ESlateVisibility::Hidden);return;
    }
    WarningText->SetVisibility(ESlateVisibility::HitTestInvisible);
    bool bPlay=false;
    if(Seconds<=10 && !bWarned10){bWarned30=bWarned10=true;bPlay=true;}
    else if(Seconds<=30 && !bWarned30){bWarned30=true;bPlay=true;}
    if(bPlay && WarningSound)UGameplayStatics::PlaySound2D(this,WarningSound,.65f);
    if(Seconds==LastSecond)return;
    LastSecond=Seconds;
    WarningText->SetText(FText::Format(NSLOCTEXT("DreamUI","RemainingWarning","남은 시간 {0}초"),FText::AsNumber(Seconds)));
    WarningText->SetColorAndOpacity(Seconds<=10?FLinearColor(1,.15f,.1f,1):FLinearColor(.95f,.65f,.25f,1));
}

FText UDreamTimeWarningWidget::GetDisplayText() const
{
    return WarningText?WarningText->GetText():FText::GetEmpty();
}
