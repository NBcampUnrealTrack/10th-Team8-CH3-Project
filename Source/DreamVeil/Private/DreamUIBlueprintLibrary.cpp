#include "DreamUIBlueprintLibrary.h"
#include "BedActor.h"
#include "DreamVeilGameInstance.h"
#include "MainPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"

void UDreamUIBlueprintLibrary::OpenBedDreamSelect(UUserWidget* Widget)
{
    if (!Widget) return;
    if (auto* Bed = Cast<ABedActor>(UGameplayStatics::GetActorOfClass(Widget, ABedActor::StaticClass())))
        Bed->OpenDreamSelect();
}

void UDreamUIBlueprintLibrary::PlayMainMenuBGM(UUserWidget* Widget)
{
    if (!Widget) return;
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
