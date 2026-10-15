#include "DreamEnhancePreviewWidget.h"
#include "InventoryComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "UObject/UnrealType.h"
#include "Styling/CoreStyle.h"

void UDreamEnhancePreviewWidget::Install(UUserWidget* Host)
{
    if(!Host || !Host->WidgetTree)return;
    auto* Info=Cast<UTextBlock>(Host->GetWidgetFromName(TEXT("Text_EnhanceInfo")));
    auto* Canvas=Info?Cast<UCanvasPanel>(Info->GetParent()):nullptr;
    if(!Canvas || Host->GetWidgetFromName(TEXT("EnhancePreviewNative")))return;
    auto* Preview=Host->WidgetTree->ConstructWidget<UDreamEnhancePreviewWidget>(StaticClass(),TEXT("EnhancePreviewNative"));
    Preview->HostWidget=Host;
    Preview->SetOwningPlayer(Host->GetOwningPlayer());
    Preview->BuildPreview();
    if(Preview->PreviewText){auto Font=Info->GetFont();Font.Size=15;Preview->PreviewText->SetFont(Font);}
    auto* PreviewSlot=Canvas->AddChildToCanvas(Preview);
    PreviewSlot->SetPosition(FVector2D(26,166));PreviewSlot->SetSize(FVector2D(381,96));
    PreviewSlot->SetZOrder(3);
    // Old cost/probability text is superseded by the same values plus an explicit stat comparison.
    Info->SetVisibility(ESlateVisibility::Collapsed);
}
void UDreamEnhancePreviewWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();BuildPreview();
}
void UDreamEnhancePreviewWidget::BuildPreview()
{
    if(!WidgetTree)WidgetTree=NewObject<UWidgetTree>(this,TEXT("WidgetTree"));
    if(WidgetTree->RootWidget)return;
    PreviewText=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("EnhancePreview_Text"));
    WidgetTree->RootWidget=PreviewText;PreviewText->SetAutoWrapText(true);
    PreviewText->SetColorAndOpacity(FLinearColor(.88f,.86f,.80f,1));
    if(auto* Host=HostWidget.Get())
        if(auto* Info=Cast<UTextBlock>(Host->GetWidgetFromName(TEXT("Text_EnhanceInfo"))))
        {auto Font=Info->GetFont();Font.Size=15;PreviewText->SetFont(Font);}
    SetVisibility(ESlateVisibility::HitTestInvisible);
}
void UDreamEnhancePreviewWidget::NativeConstruct()
{
    Super::NativeConstruct();BuildPreview();LastIndex=LastEnhance=LastShards=INDEX_NONE;RefreshElapsed=0;Refresh();
}
void UDreamEnhancePreviewWidget::NativeTick(const FGeometry& Geometry,float DeltaTime)
{
    Super::NativeTick(Geometry,DeltaTime);RefreshElapsed+=DeltaTime;
    if(RefreshElapsed<.15f)return;RefreshElapsed=0;Refresh();
}
void UDreamEnhancePreviewWidget::Refresh()
{
    auto* Host=HostWidget.Get();const auto* Pawn=GetOwningPlayerPawn();
    const auto* Inventory=Pawn?Pawn->FindComponentByClass<UInventoryComponent>():nullptr;
    const auto* IndexProperty=Host?FindFProperty<FIntProperty>(Host->GetClass(),TEXT("SelectedEnhancePartIndex")):nullptr;
    if(!PreviewText)return;
    const int32 Index=IndexProperty?IndexProperty->GetPropertyValue_InContainer(Host):INDEX_NONE;
    if(!Inventory || !Inventory->GetPartsView().IsValidIndex(Index))
    {PreviewText->SetText(FText::GetEmpty());LastIndex=INDEX_NONE;return;}
    const FWeaponPart& Part=Inventory->GetPartsView()[Index];const int32 Shards=Inventory->GetDreamShards();
    if(Index==LastIndex && Part.EnhanceLevel==LastEnhance && Part.Slot==LastSlot && Part.Tier==LastTier && Shards==LastShards)return;
    LastIndex=Index;LastEnhance=Part.EnhanceLevel;LastSlot=Part.Slot;LastTier=Part.Tier;LastShards=Shards;
    if(Part.EnhanceLevel<0 || uint8(Part.Tier)>uint8(EWeaponPartTier::Boss))
    {PreviewText->SetText(FText::GetEmpty());return;}
    FWeaponPart Next=Part;Next.EnhanceLevel=FMath::Min(Part.EnhanceLevel+1,MAX_PART_ENHANCE_LEVEL);
    const bool Maxed=Part.EnhanceLevel>=MAX_PART_ENHANCE_LEVEL;
    const bool Damage=IsDamagePartSlot(Part.Slot);
    const float Before=Damage?CalculatePartDamageBonus(Part):CalculatePartFireRateBonus(Part)*100;
    const float After=Damage?CalculatePartDamageBonus(Next):CalculatePartFireRateBonus(Next)*100;
    const int32 Cost=Maxed?0:UInventoryComponent::GetEnhanceCost(Part);
    const float Chance=UInventoryComponent::GetEnhanceSuccessChance(Part);
    const float Destruction=(1-Chance)*UInventoryComponent::GetEnhanceDestroyChance(Part);
    const FString Summary=FString::Printf(TEXT("%s  %.2f → %.2f%s\n강화 비용  %d     보유  %d\n성공  %.1f%%     시도당 파괴  %.1f%%\n%s"),
        Damage?TEXT("공격력 보너스"):TEXT("발사 간격 감소"),Before,After,Damage?TEXT(""):TEXT("%"),Cost,Shards,Chance*100,Destruction*100,
        Maxed?TEXT("최대 강화 단계입니다"):(Shards>=Cost?TEXT("강화 가능 · 성공 시 예상 수치"):TEXT("꿈의 조각이 부족합니다")));
    PreviewText->SetText(FText::FromString(Summary));
}

FText UDreamEnhancePreviewWidget::GetDisplayText() const
{
    return PreviewText?PreviewText->GetText():FText::GetEmpty();
}

void UDreamEnhancePreviewWidget::RefreshPreview()
{
    BuildPreview();LastIndex=INDEX_NONE;Refresh();
}
