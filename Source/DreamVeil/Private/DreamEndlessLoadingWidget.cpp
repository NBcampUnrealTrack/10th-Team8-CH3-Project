#include "DreamEndlessLoadingWidget.h"
#include "DreamVeilGameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Engine/Font.h"
#include "HAL/PlatformTime.h"
void UDreamEndlessLoadingWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    SetVisibility(ESlateVisibility::Visible);SetIsFocusable(true);
    auto* Root=WidgetTree->ConstructWidget<UOverlay>();WidgetTree->RootWidget=Root;
    auto* Black=WidgetTree->ConstructWidget<UBorder>();Black->SetBrushColor(FLinearColor::Black);
    auto* BlackSlot=Root->AddChildToOverlay(Black);BlackSlot->SetHorizontalAlignment(HAlign_Fill);BlackSlot->SetVerticalAlignment(VAlign_Fill);
    auto* Scale=WidgetTree->ConstructWidget<UScaleBox>();Scale->SetStretch(EStretch::ScaleToFit);
    auto* VideoSlot=Root->AddChildToOverlay(Scale);VideoSlot->SetHorizontalAlignment(HAlign_Fill);VideoSlot->SetVerticalAlignment(VAlign_Fill);
    auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(1280);Size->SetHeightOverride(720);Scale->SetContent(Size);
    auto* Image=WidgetTree->ConstructWidget<UImage>();Size->SetContent(Image);
    Player=NewObject<UMediaPlayer>(this);Player->SetLooping(true);
    Texture=NewObject<UMediaTexture>(this);Texture->NewStyleOutput=true;Texture->SetMediaPlayer(Player);Texture->UpdateResource();
    auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/UI/Materials/M_UI_EndlessLoading.M_UI_EndlessLoading"));
    if(Material){VideoMaterial=UMaterialInstanceDynamic::Create(Material,this);VideoMaterial->SetTextureParameterValue(TEXT("Video"),Texture);Image->SetBrushFromMaterial(VideoMaterial);}
    auto* Caption=WidgetTree->ConstructWidget<UTextBlock>();Caption->SetText(FText::FromString(TEXT("ENDLESS  ·  악몽의 도시로 이동 중…")));
    Caption->SetFont(FSlateFontInfo(LoadObject<UFont>(nullptr,TEXT("/Game/UI/Fonts/Pretendard-SemiBold_Font.Pretendard-SemiBold_Font")),20));Caption->SetColorAndOpacity(FLinearColor(.9f,.75f,1));
    auto* TextSlot=Root->AddChildToOverlay(Caption);TextSlot->SetHorizontalAlignment(HAlign_Center);TextSlot->SetVerticalAlignment(VAlign_Bottom);TextSlot->SetPadding(FMargin(24,0,24,50));
}
void UDreamEndlessLoadingWidget::NativeConstruct()
{
    Super::NativeConstruct();Started=FPlatformTime::Seconds();FirstPlaying=0;bTravelled=false;
    bWasPaused=UGameplayStatics::IsGamePaused(this);UGameplayStatics::SetGamePaused(this,true);SetKeyboardFocus();
    Player->OpenFile(FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()/TEXT("Movies/EndlessTransition.mp4")));
}
bool UDreamEndlessLoadingWidget::IsVideoPlaying() const{return Player&&Player->IsPlaying();}
float UDreamEndlessLoadingWidget::GetVideoTime() const{return Player?Player->GetTime().GetTotalSeconds():0;}
void UDreamEndlessLoadingWidget::NativeTick(const FGeometry& G,float DT)
{
    Super::NativeTick(G,DT);if(bTravelled)return;
    const double Now=FPlatformTime::Seconds();if(IsVideoPlaying()&&FirstPlaying==0)FirstPlaying=Now;
    // A media-open failure has a bounded fallback; it never traps the player in a loading screen.
    if((FirstPlaying>0&&Now-FirstPlaying>=4.)||Now-Started>=7.)
    {
        bTravelled=true;UGameplayStatics::SetGamePaused(this,false);
        UE_LOG(LogTemp,Display,TEXT("EndlessTransition PIE preview completed; travelling to city"));
        UGameplayStatics::OpenLevel(this,TEXT("/Game/Endless/Endless"));
    }
}
void UDreamEndlessLoadingWidget::NativeDestruct()
{if(Player)Player->Close();if(!bTravelled&&!bWasPaused&&GetWorld())UGameplayStatics::SetGamePaused(this,false);Super::NativeDestruct();}
