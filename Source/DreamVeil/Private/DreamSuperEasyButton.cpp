#include "DreamSuperEasyButton.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

void UDreamSuperEasyButton::Install(UUserWidget* Host)
{
    if(!Host || !Host->WidgetTree)return;
    if(Host->GetWidgetFromName(TEXT("Button_SuperEasy")))return;
    auto* Easy=Cast<UButton>(Host->GetWidgetFromName(TEXT("Button_Easy")));
    auto* Canvas=Easy?Cast<UCanvasPanel>(Easy->GetParent()):nullptr;
    auto* GI=Host->GetGameInstance<UDreamVeilGameInstance>();
    if(!Canvas || !GI)return;
    auto* Button=Host->WidgetTree->ConstructWidget<UDreamSuperEasyButton>(StaticClass(),TEXT("Button_SuperEasy"));
    Button->HostWidget=Host;Button->SetStyle(Easy->GetStyle());
    auto* Caption=Host->WidgetTree->ConstructWidget<UTextBlock>();
    if(auto* Existing=Cast<UTextBlock>(Easy->GetContent()))
    {
        auto Font=Existing->GetFont();Font.Size=18;Caption->SetFont(Font);
        Caption->SetColorAndOpacity(Existing->GetColorAndOpacity());
    }
    Caption->SetText(NSLOCTEXT("DreamUI","SuperEasyName","슈퍼 겁쟁이 모드"));Caption->SetJustification(ETextJustify::Center);
    Button->SetContent(Caption);Canvas->AddChildToCanvas(Button);
    const TCHAR* Names[]={TEXT("Button_SuperEasy"),TEXT("Button_Easy"),TEXT("Button_Normal"),TEXT("Button_Hard")};
    for(int32 Index=0;Index<4;++Index)
    {
        auto* Item=Host->GetWidgetFromName(Names[Index]);
        if(auto* CanvasSlot=Item?Cast<UCanvasPanelSlot>(Item->Slot):nullptr)
        {
            CanvasSlot->SetPosition(FVector2D(Index*194.0f,0));CanvasSlot->SetSize(FVector2D(176,56));
        }
    }
    Button->OnClicked.AddUniqueDynamic(Button,&UDreamSuperEasyButton::SelectSuperEasy);
    GI->OnDifficultyChanged.AddUniqueDynamic(Button,&UDreamSuperEasyButton::RefreshSelection);
    Button->RefreshSelection(GI->GetDifficulty());
}
void UDreamSuperEasyButton::SelectSuperEasy()
{
    auto* Host=HostWidget.Get();if(!Host)return;
    auto* GI=Host->GetGameInstance<UDreamVeilGameInstance>();if(!GI)return;
    GI->SetDifficulty(EGameDifficulty::SuperEasy);
    if(UFunction* Refresh=Host->FindFunction(TEXT("RefreshDifficulty")))Host->ProcessEvent(Refresh,nullptr);
    RefreshSelection(GI->GetDifficulty());
}
void UDreamSuperEasyButton::RefreshSelection(EGameDifficulty Difficulty)
{
    const bool Selected=Difficulty==EGameDifficulty::SuperEasy;
    SetBackgroundColor(Selected?FLinearColor(.9f,.65f,.28f,1):FLinearColor::White);
    auto* Host=HostWidget.Get();if(!Selected || !Host)return;
    for(const TCHAR* Name:{TEXT("Button_Easy"),TEXT("Button_Normal"),TEXT("Button_Hard")})
        if(auto* Button=Cast<UButton>(Host->GetWidgetFromName(Name)))Button->SetBackgroundColor(FLinearColor::White);
    if(auto* Info=Cast<UTextBlock>(Host->GetWidgetFromName(TEXT("Text_DiffInfo"))))
        Info->SetText(NSLOCTEXT("DreamUI","SuperEasyDescription","당신은 슈퍼 겁쟁이 입니까?"));
}
