#include "DreamUIBlueprintLibrary.h"
#include "MainPlayerCharacter.h"
#include "DreamAugmentPanel.h"
#include "Containers/Ticker.h"
#include "DreamSuperEasyButton.h"
#include "DreamEnhancePreviewWidget.h"
#include "InventoryComponent.h"
#include "UObject/UnrealType.h"
#include "DreamObjectPoolSubsystem.h"
#include "Components/PanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ButtonSlot.h"
#include "Components/ScaleBox.h"
#include "Engine/World.h"
#include "BedActor.h"
#include "DreamVeilGameInstance.h"
#include "MainPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"

void UDreamUIBlueprintLibrary::FitMenuLabels(UUserWidget* Host)
{
    if(!Host||!Host->WidgetTree)return;
    TArray<UWidget*> Widgets;Host->WidgetTree->GetAllWidgets(Widgets);
    auto SingleLine=[](UTextBlock* Text)
    {
        Text->SetAutoWrapText(false);Text->SetWrapTextAt(0);
        auto Font=Text->GetFont();Font.Size=20;Text->SetFont(Font);
        const FString Original=Text->GetText().ToString();
        if(Original.Contains(TEXT("\n")))Text->SetText(FText::FromString(Original.Replace(TEXT("\r"),TEXT("")).Replace(TEXT("\n"),TEXT(" "))));
    };
    for(auto* Widget:Widgets)
    {
        if(auto* Button=Cast<UButton>(Widget))
        {
            // Card contents are panels with long descriptions; only standalone captions fit here.
            auto* Text=Cast<UTextBlock>(Button->GetContent());
            if(auto* LegacyFit=Cast<UScaleBox>(Button->GetContent()))
                if(auto* LegacyText=Cast<UTextBlock>(LegacyFit->GetContent())){LegacyFit->SetContent(nullptr);Button->SetContent(LegacyText);Text=LegacyText;}
            if(Text)
            {
                SingleLine(Text);
                const auto* CatalogButton=Cast<UDreamAugmentActionButton>(Button);
                if(!CatalogButton||CatalogButton->Action<1000||CatalogButton->Action>=1256)Text->SetJustification(ETextJustify::Center);
                auto Style=Button->GetStyle();Style.NormalPadding=Style.PressedPadding=FMargin(8,4);Button->SetStyle(Style);
                if(auto* Slot=Cast<UButtonSlot>(Text->Slot)){Slot->SetPadding(FMargin(0));Slot->SetHorizontalAlignment(HAlign_Fill);Slot->SetVerticalAlignment(VAlign_Center);}
            }
            continue;
        }
    }
}

void UDreamUIBlueprintLibrary::OpenBedDreamSelect(UUserWidget* Widget)
{
    if (!Widget) return;
    if (auto* Bed = Cast<ABedActor>(UGameplayStatics::GetActorOfClass(Widget, ABedActor::StaticClass())))
        Bed->OpenDreamSelect();
}

void UDreamUIBlueprintLibrary::PlayMainMenuBGM(UUserWidget* Widget)
{
    if (!Widget) return;
    UDreamSuperEasyButton::Install(Widget);
    FitMenuLabels(Widget);
    if (auto* GI = Widget->GetGameInstance<UDreamVeilGameInstance>())
        GI->PlayMainMenuBGM();
}

void UDreamUIBlueprintLibrary::CloseBedDreamMenu(UUserWidget* Widget)
{
    if (!Widget) return;
    if (auto* Bed = Cast<ABedActor>(UGameplayStatics::GetActorOfClass(Widget, ABedActor::StaticClass())))
        Bed->CloseBedMenu();
}

void UDreamUIBlueprintLibrary::RefreshDreamSelection(UUserWidget* Widget)
{
    if (!Widget) return;
    auto* GI = Widget->GetGameInstance<UDreamVeilGameInstance>();
    for (int32 Index = 0; Index < 5; ++Index)
    {
        const int32 Level = Index + 1;
        const bool bEndless = Level == 5;
        const bool bUnlocked = GI && (bEndless ? GI->IsEndlessUnlocked() : GI->IsLevelUnlocked(Level));
        const bool bCleared = GI && !bEndless && GI->IsLevelCleared(Level);
        if (auto* Button = Cast<UButton>(Widget->GetWidgetFromName(*FString::Printf(TEXT("Button_Level%d"), Level))))
            Button->SetIsEnabled(bUnlocked);
        if (auto* Label = Cast<UTextBlock>(Widget->GetWidgetFromName(*FString::Printf(TEXT("Text_Level%d"), Level))))
        {
            FString Caption = bEndless ? TEXT("Endless") : FString::Printf(TEXT("L%d"), Level);
            if (bCleared) Caption += TEXT(" · 클리어");
            else if (!bUnlocked) Caption += TEXT(" · 잠김");
            Label->SetText(FText::FromString(Caption));
        }
    }
}

bool UDreamUIBlueprintLibrary::EnterDream(UUserWidget* Widget, int32 LevelNumber)
{
    auto* GI = Widget ? Widget->GetGameInstance<UDreamVeilGameInstance>() : nullptr;
    if (!GI) return false;
    return LevelNumber == 0 ? GI->OpenEndless() : GI->OpenLevelByNumber(LevelNumber);
}

void UDreamUIBlueprintLibrary::ApplyAugmentIcons(UUserWidget* Widget, const TArray<EAugmentID>& Choices, UDreamAugmentIconSet* IconSet)
{
    if (!Widget) return;
    auto* Pawn=Cast<AMainPlayerCharacter>(Widget->GetOwningPlayerPawn());
    // Core ticker uses real time while the choice pauses world simulation.
    if (Pawn && !Pawn->IsAugmentChoiceInputReady())
    {
        TArray<TWeakObjectPtr<UButton>> Buttons;
        for (int32 Index=1;Index<=3;++Index)
            if (auto* Button=Cast<UButton>(Widget->GetWidgetFromName(*FString::Printf(TEXT("Button_Choice%d"),Index))))
            { Button->SetIsEnabled(false); Buttons.Add(Button); }
        TWeakObjectPtr<AMainPlayerCharacter> WeakPawn=Pawn;
        TWeakObjectPtr<UUserWidget> WeakWidget=Widget;
        const int32 ChoiceCount=Choices.Num();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPawn,WeakWidget,Buttons,ChoiceCount](float)
        {
            if (!WeakPawn.IsValid() || !WeakWidget.IsValid() || !WeakPawn->HasAugmentChoices()) return false;
            if (!WeakPawn->IsAugmentChoiceInputReady()) return true;
            for (int32 I=0;I<Buttons.Num();++I) if (Buttons[I].IsValid()) Buttons[I]->SetIsEnabled(I<ChoiceCount);
            return false;
        }),.05f);
    }
    for (int32 Index = 0; Index < 3; ++Index)
    {
        auto* Image = Cast<UImage>(Widget->GetWidgetFromName(*FString::Printf(TEXT("Image_Choice%dIcon"), Index + 1)));
        if (!Image) continue;
        UTexture2D* Texture = nullptr;
        if (IconSet && Choices.IsValidIndex(Index))
            if (const auto* Found = IconSet->Icons.Find(Choices[Index])) Texture = Found->Get();
        Image->SetBrushFromTexture(Texture, false);
        Image->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }
    UDreamAugmentPanel::InstallChoiceToolbar(Widget);
    FitMenuLabels(Widget);
}

void UDreamUIBlueprintLibrary::ContinueDreamGameOver(UUserWidget* Widget)
{
    if (!Widget) return;
    auto* PC = Cast<AMainPlayerController>(Widget->GetOwningPlayer());
    auto* GI = Widget->GetGameInstance<UDreamVeilGameInstance>();
    if (!PC || !GI) return;
    const bool bTimedOut = PC->IsCorruptionGameOver();
    PC->CloseMenuWidget();
    if (bTimedOut) GI->FailCurrentLevel();
    else GI->ContinueAfterDeath();
}

UUserWidget* UDreamUIBlueprintLibrary::AcquirePooledWidget(UObject* Context, TSubclassOf<UUserWidget> WidgetClass, APlayerController* OwningPlayer)
{
    UWorld* World = Context ? Context->GetWorld() : nullptr;
    if (!World) return nullptr;
    if (!OwningPlayer) OwningPlayer = UGameplayStatics::GetPlayerController(Context, 0);
    UDreamObjectPoolSubsystem* Pool = World->GetSubsystem<UDreamObjectPoolSubsystem>();
    UUserWidget* Result = Pool ? Pool->AcquireWidget(Context, WidgetClass, OwningPlayer) : nullptr;
    if (Result && WidgetClass && WidgetClass->GetFName() == TEXT("WBP_Computer_C")) UDreamEnhancePreviewWidget::Install(Result);
    return Result;
}
void UDreamUIBlueprintLibrary::ReleasePooledWidget(UUserWidget* Widget)
{
    if (!Widget) return;
    UDreamObjectPoolSubsystem* Pool = Widget->GetWorld() ? Widget->GetWorld()->GetSubsystem<UDreamObjectPoolSubsystem>() : nullptr;
    if (!Pool || !Pool->ReleaseWidget(Widget)) Widget->RemoveFromParent();
}
void UDreamUIBlueprintLibrary::ReleasePooledPanelChildren(UPanelWidget* Panel)
{
    if (!Panel) return;
    while (Panel->GetChildrenCount())
    {
        UWidget* Child = Panel->GetChildAt(Panel->GetChildrenCount() - 1);
        if (UUserWidget* Widget = Cast<UUserWidget>(Child)) ReleasePooledWidget(Widget);
        else Panel->RemoveChild(Child);
    }
}

UWidget* UDreamUIBlueprintLibrary::FindNamedWidget(UUserWidget* Host,FName Name)
{
    return Host?Host->GetWidgetFromName(Name):nullptr;
}

bool UDreamUIBlueprintLibrary::SelectEnhancePart(UUserWidget* Host,int32 PartIndex)
{
    if(!Host || !Host->GetOwningPlayerPawn())return false;
    const auto* Inventory=Host->GetOwningPlayerPawn()->FindComponentByClass<UInventoryComponent>();
    auto* Selected=FindFProperty<FIntProperty>(Host->GetClass(),TEXT("SelectedEnhancePartIndex"));
    if(!Inventory || !Selected)return false;
    const bool Valid=Inventory->GetPartsView().IsValidIndex(PartIndex);
    Selected->SetPropertyValue_InContainer(Host,Valid?PartIndex:INDEX_NONE);
    if(UFunction* Refresh=Host->FindFunction(TEXT("RefreshEnhanceDetail")))Host->ProcessEvent(Refresh,nullptr);
    if(auto* Preview=Cast<UDreamEnhancePreviewWidget>(Host->GetWidgetFromName(TEXT("EnhancePreviewNative"))))Preview->RefreshPreview();
    return Valid;
}
