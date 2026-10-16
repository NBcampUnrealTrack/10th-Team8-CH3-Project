#include "DreamLobbyWidgets.h"
#include "DreamTrainingSystem.h"
#include "DreamVeilGameInstance.h"
#include "DreamAugmentCatalog.h"
#include "DispatchTableComponent.h"
#include "InventoryComponent.h"
#include "MainPlayerCharacter.h"
#include "MainPlayerController.h"
#include "WeaponBase.h"
#include "CombatStatsComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
namespace { FSlateChildSize Weighted(float Value){FSlateChildSize S(ESlateSizeRule::Fill);S.Value=Value;return S;} }
void UDreamLobbyButton::Configure(UDreamLobbyPanel* Owner,int32 Command){Panel=Owner;Action=Command;OnClicked.AddUniqueDynamic(this,&UDreamLobbyButton::Invoke);}
void UDreamLobbyButton::OnSlotAdded(UPanelSlot* AddedSlot)
{Super::OnSlotAdded(AddedSlot);if(auto* ContentSlot=Cast<UButtonSlot>(AddedSlot)){ContentSlot->SetHorizontalAlignment(HAlign_Fill);ContentSlot->SetVerticalAlignment(VAlign_Center);}}
void UDreamLobbyButton::Invoke(){if(Panel)Panel->HandleAction(Action);}
UDreamLobbyPanel::UDreamLobbyPanel(const FObjectInitializer& I):Super(I)
{
    SetIsFocusable(true);
    static ConstructorHelpers::FObjectFinder<UFont> F(TEXT("/Game/UI/Fonts/Pretendard-SemiBold_Font"));Font=F.Object;
    static ConstructorHelpers::FObjectFinder<UTexture2D> A(TEXT("/Game/UI/DreamVeilSkin/Frames/T_Frame_Panel_9Slice"));Frame=A.Object;
    static ConstructorHelpers::FObjectFinder<UTexture2D> B(TEXT("/Game/UI/DreamVeilSkin/Buttons/T_Button_Normal"));Normal=B.Object;
    static ConstructorHelpers::FObjectFinder<UTexture2D> C(TEXT("/Game/UI/DreamVeilSkin/Buttons/T_Button_Hovered"));Hover=C.Object;
    static ConstructorHelpers::FObjectFinder<UTexture2D> D(TEXT("/Game/UI/DreamVeilSkin/Buttons/T_Button_Pressed"));Press=D.Object;
}
UTextBlock* UDreamLobbyPanel::Label(const FString& Text,int32 Size)
{
    auto* W=WidgetTree->ConstructWidget<UTextBlock>();W->SetText(FText::FromString(Text));W->SetFont(FSlateFontInfo(Font.Get(),Size));
    W->SetColorAndOpacity(FLinearColor(.94f,.89f,.77f));W->SetAutoWrapText(true);return W;
}
UDreamLobbyButton* UDreamLobbyPanel::Button(UVerticalBox* Box,const FString& Text,int32 Action,bool Enabled)
{
    auto* W=WidgetTree->ConstructWidget<UDreamLobbyButton>();W->Configure(this,Action);
    auto Brush=[](UTexture2D* T){FSlateBrush B;B.SetResourceObject(T);B.DrawAs=ESlateBrushDrawType::Box;B.Margin=FMargin(.12f);return B;};
    FButtonStyle Style;Style.Normal=Brush(Normal);Style.Hovered=Brush(Hover);Style.Pressed=Brush(Press);Style.NormalPadding=Style.PressedPadding=FMargin(12,8);W->SetStyle(Style);
    auto* TextBlock=Label(Text);TextBlock->SetAutoWrapText(false);TextBlock->SetJustification(ETextJustify::Center);W->SetContent(TextBlock);W->SetIsEnabled(Enabled);
    auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetMinDesiredHeight(50);Size->SetContent(W);Box->AddChildToVerticalBox(Size)->SetPadding(FMargin(0,3));return W;
}
void UDreamLobbyPanel::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    auto* Canvas=WidgetTree->ConstructWidget<UCanvasPanel>();WidgetTree->RootWidget=Canvas;
    auto* Scale=WidgetTree->ConstructWidget<UScaleBox>();Scale->SetStretch(EStretch::ScaleToFit);Scale->SetStretchDirection(EStretchDirection::DownOnly);
    auto* SlotIndex=Canvas->AddChildToCanvas(Scale);SlotIndex->SetAnchors(FAnchors(0,0,1,1));SlotIndex->SetOffsets(FMargin(24));
    auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(1100);Size->SetHeightOverride(780);Scale->SetContent(Size);
    auto* Surface=WidgetTree->ConstructWidget<UBorder>();Surface->SetBrushColor(FLinearColor(.055f,.035f,.085f,.97f));Surface->SetPadding(FMargin(0));Size->SetContent(Surface);
    auto* Border=WidgetTree->ConstructWidget<UBorder>();FSlateBrush B;B.SetResourceObject(Frame);B.DrawAs=ESlateBrushDrawType::Box;B.Margin=FMargin(.0625f);Border->SetBrush(B);Border->SetPadding(FMargin(32));Surface->SetContent(Border);
    auto* Rows=WidgetTree->ConstructWidget<UVerticalBox>();Border->SetContent(Rows);
    Title=Label(TEXT(""),26);Rows->AddChildToVerticalBox(Title)->SetPadding(FMargin(0,0,0,8));
    Wallet=Label(TEXT(""),18);Rows->AddChildToVerticalBox(Wallet)->SetPadding(FMargin(0,0,0,12));
    Content=WidgetTree->ConstructWidget<UVerticalBox>();auto* Main=Rows->AddChildToVerticalBox(Content);Main->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    Status=Label(TEXT(""),18);Rows->AddChildToVerticalBox(Status)->SetPadding(FMargin(0,8));
    Button(Rows,TEXT("닫기"),-1);
}
void UDreamLobbyPanel::NativeConstruct()
{
    Super::NativeConstruct();auto* P=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());Inventory=P?P->Inventory.Get():nullptr;
    if(Inventory.IsValid())Inventory->OnInventoryChanged.AddUniqueDynamic(this,&UDreamLobbyPanel::RefreshPanel);
    RefreshPanel();
}
void UDreamLobbyPanel::NativeDestruct(){if(Inventory.IsValid())Inventory->OnInventoryChanged.RemoveDynamic(this,&UDreamLobbyPanel::RefreshPanel);Inventory.Reset();Super::NativeDestruct();}
void UDreamLobbyPanel::RefreshPanel()
{
    if(!Content)return;Content->ClearChildren();auto* P=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());
    Wallet->SetText(FText::FromString(FString::Printf(TEXT("꿈의 조각  %s"),*FText::AsNumber(P&&P->Inventory?P->Inventory->GetDreamShards():0).ToString())));
}
void UDreamLobbyPanel::HandleAction(int32 Action){if(Action==-1)if(auto* PC=Cast<AMainPlayerController>(GetOwningPlayer()))PC->CloseMenuWidget();}
FReply UDreamLobbyPanel::NativeOnPreviewKeyDown(const FGeometry& G,const FKeyEvent& E)
{
    if(E.GetKey()==EKeys::Escape||E.GetKey()==EKeys::Z||(IsA<UDreamInventoryWidget>()&&E.GetKey()==EKeys::I))
    {if(!E.IsRepeat())HandleAction(-1);return FReply::Handled();}
    return Super::NativeOnPreviewKeyDown(G,E);
}
UTexture2D* UDreamLobbyPanel::PartIcon(int32 SlotIndex)const
{
    const TCHAR* Names[]={TEXT("Muzzle"),TEXT("Magazine"),TEXT("Sight"),TEXT("Stock"),TEXT("Foregrip")};
    return LoadObject<UTexture2D>(nullptr,*(FString(TEXT("/Game/UI/DreamVeilSkin/Icons/T_Icon_"))+Names[FMath::Clamp(SlotIndex,0,4)]));
}
UHorizontalBox* UDreamLobbyPanel::PartRow(const FString& Text,UTexture2D* Icon)
{
    auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();auto* Size=WidgetTree->ConstructWidget<USizeBox>();Size->SetWidthOverride(44);Size->SetHeightOverride(44);
    auto* Image=WidgetTree->ConstructWidget<UImage>();Image->SetBrushFromTexture(Icon);if(!Icon)Image->SetVisibility(ESlateVisibility::Hidden);Size->SetContent(Image);Row->AddChildToHorizontalBox(Size)->SetPadding(FMargin(0,0,10,0));
    auto* Caption=Label(Text,18);Caption->SetAutoWrapText(false);auto* TextSlot=Row->AddChildToHorizontalBox(Caption);TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));TextSlot->SetVerticalAlignment(VAlign_Center);return Row;
}
void UDreamTrainingWidget::RefreshPanel()
{
    Super::RefreshPanel();if(!Content)return;Title->SetText(FText::FromString(TEXT("꿈의 단련소")));
    auto* P=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());auto* T=GetGameInstance()?GetGameInstance()->GetSubsystem<UDreamTrainingSystem>():nullptr;
    if(!P||!P->Inventory||!P->DispatchTable||!T)return;
    auto* Columns=WidgetTree->ConstructWidget<UHorizontalBox>();Content->AddChildToVerticalBox(Columns)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* Left=WidgetTree->ConstructWidget<UVerticalBox>();auto* L=Columns->AddChildToHorizontalBox(Left);L->SetSize(Weighted(.43f));L->SetPadding(FMargin(0,0,28,0));
    Left->AddChildToVerticalBox(Label(TEXT("영구 단련"),22));Left->AddChildToVerticalBox(Label(TEXT("능력별 단계와 총 단련 횟수에 따라 비용이 증가합니다."),16))->SetPadding(FMargin(0,4,0,10));
    const TCHAR* Names[]={TEXT("최대 체력 +10"),TEXT("방어력 +1"),TEXT("공격력 +1"),TEXT("최대 스태미나 +5")};
    for(int32 I=0;I<4;++I)
    {
        const auto Stat=static_cast<EDreamTrainingStat>(I);const int32 Cost=T->GetUpgradeCost(Stat);
        Left->AddChildToVerticalBox(Label(FString::Printf(TEXT("%s   ·   Lv.%d"),Names[I],T->GetLevel(Stat)),20))->SetPadding(FMargin(0,6,0,0));
        Button(Left,FString::Printf(TEXT("단련 · %s 조각"),*FText::AsNumber(Cost).ToString()),100+I,P->Inventory->GetDreamShards()>=Cost);
    }
    auto* Right=WidgetTree->ConstructWidget<UVerticalBox>();auto* R=Columns->AddChildToHorizontalBox(Right);R->SetSize(Weighted(.57f));
    Right->AddChildToVerticalBox(Label(TEXT("증강 정리"),22));
    Right->AddChildToVerticalBox(Label(TEXT("선택한 증강의 모든 중첩을 제거합니다.\n진화 재료를 제거하면 해당 진화와 스킬도 해제됩니다."),16))->SetPadding(FMargin(0,4,0,8));
    auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();Right->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    auto* List=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(List);TSet<EAugmentID> Seen;
    for(auto ID:P->DispatchTable->GetAugmentHistory())
    {
        if(Seen.Contains(ID))continue;Seen.Add(ID);const auto* Def=DreamAugmentCatalog::Find(ID);if(!Def)continue;
        const FString Text=FString::Printf(TEXT("%s%s · %d중첩"),static_cast<int32>(ID)==SelectedAugment?TEXT("선택 · "):TEXT(""),*Def->Name,P->DispatchTable->GetStackCount(ID));
        Button(List,Text,1000+static_cast<int32>(ID),P->DispatchTable->CanRemoveAugment(ID));
    }
    if(Seen.IsEmpty())List->AddChildToVerticalBox(Label(TEXT("보유 중인 증강이 없습니다."),18));
    const auto ID=static_cast<EAugmentID>(SelectedAugment);const bool Selected=SelectedAugment>=0&&P->DispatchTable->CanRemoveAugment(ID);
    const auto* Def=Selected?DreamAugmentCatalog::Find(ID):nullptr;const int32 Cost=T->GetRemovalCost();
    FString Consequences=Def?Def->Name+TEXT(" · 모든 중첩 제거 / 재획득 가능"):TEXT("제거할 증강을 목록에서 선택하세요.");
    if(Selected){const auto Removed=P->DispatchTable->GetRemovalConsequences(ID);for(int32 I=1;I<Removed.Num();++I)Consequences+=TEXT("\n함께 해제: ")+P->DispatchTable->GetAugmentDisplayName(Removed[I]).ToString();}
    Right->AddChildToVerticalBox(Label(Consequences,18))->SetPadding(FMargin(0,8));
    Button(Right,FString::Printf(TEXT("선택 증강 제거 · %s 조각"),*FText::AsNumber(Cost).ToString()),50,Selected&&P->Inventory->GetDreamShards()>=Cost);
    Status->SetText(T->GetMessage().IsEmpty()?FText::FromString(TEXT("단련은 새 게임·사망 후에도 유지됩니다.")):T->GetMessage());
}
void UDreamTrainingWidget::HandleAction(int32 Action)
{
    auto* P=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());auto* T=GetGameInstance()?GetGameInstance()->GetSubsystem<UDreamTrainingSystem>():nullptr;
    if(Action==-1){Super::HandleAction(Action);return;}
    if(!P||!T)return;
    if(Action>=100&&Action<104)T->Upgrade(P,static_cast<EDreamTrainingStat>(Action-100));
    else if(Action>=1000&&Action<1256)SelectedAugment=Action-1000;
    else if(Action==50&&SelectedAugment>=0){T->RemoveAugment(P,static_cast<EAugmentID>(SelectedAugment));SelectedAugment=-1;}
    RefreshPanel();
}
void UDreamInventoryWidget::Install(UUserWidget* Host)
{
    if(!Host||!Host->WidgetTree)return;
    auto* Canvas=Cast<UCanvasPanel>(Host->WidgetTree->RootWidget);
    // Responsive Blueprint roots may wrap their original canvas in a ScaleBox/SizeBox.
    if(!Canvas){TArray<UWidget*> Widgets;Host->WidgetTree->GetAllWidgets(Widgets);for(auto* Item:Widgets)if(auto* Found=Cast<UCanvasPanel>(Item)){Canvas=Found;break;}}
    if(!Canvas)return;
    UDreamInventoryWidget* Panel=nullptr;
    for(auto* Child:Canvas->GetAllChildren())if(auto* Existing=Cast<UDreamInventoryWidget>(Child))Panel=Existing;else Child->SetVisibility(ESlateVisibility::Collapsed);
    if(!Panel){Panel=CreateWidget<UDreamInventoryWidget>(Host->GetOwningPlayer());auto* S=Canvas->AddChildToCanvas(Panel);S->SetAnchors(FAnchors(0,0,1,1));S->SetOffsets(FMargin(0));S->SetZOrder(100);}
    Panel->RefreshPanel();Panel->SetKeyboardFocus();
}
int32 UDreamInventoryWidget::GetVisibleOwnedCount()const
{int32 Count=0;auto* P=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());if(P&&P->Inventory)for(const auto& Part:P->Inventory->GetPartsView())if(!Part.bEquipped)++Count;return Count;}
void UDreamInventoryWidget::RefreshPanel()
{
    Super::RefreshPanel();if(!Content)return;Title->SetText(FText::FromString(TEXT("인벤토리")));
    auto* P=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());if(!P||!P->Inventory)return;const auto& Parts=P->Inventory->GetPartsView();
    auto* Columns=WidgetTree->ConstructWidget<UHorizontalBox>();Content->AddChildToVerticalBox(Columns)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    const TCHAR* SlotNames[]={TEXT("총구"),TEXT("탄창"),TEXT("조준기"),TEXT("개머리판"),TEXT("손잡이")};
    for(int32 Column=0;Column<3;++Column)
    {
        auto* Box=WidgetTree->ConstructWidget<UVerticalBox>();auto* S=Columns->AddChildToHorizontalBox(Box);S->SetSize(Weighted(Column==1?.42f:.29f));S->SetPadding(FMargin(Column?16:0,0,Column==2?0:12,0));
        if(Column==1)
        {
            Box->AddChildToVerticalBox(Label(FString::Printf(TEXT("보유 파츠 · %d"),GetVisibleOwnedCount()),22));
            Box->AddChildToVerticalBox(Label(TEXT("클릭하여 장착 · 장착한 파츠는 양옆에 표시됩니다."),16))->SetPadding(FMargin(0,4,0,12));
            auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();Box->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));auto* Rows=WidgetTree->ConstructWidget<UVerticalBox>();Scroll->AddChild(Rows);
            for(int32 I=0;I<Parts.Num();++I)if(!Parts[I].bEquipped)
            {
                const auto& Part=Parts[I];auto* B=Button(Rows,TEXT(""),10000+I);
                B->SetContent(PartRow(FString::Printf(TEXT("%s %s · %d등급 +%d"),Part.Weapon==EWeaponSlot::Pistol?TEXT("권총"):TEXT("소총"),SlotNames[static_cast<int32>(Part.Slot)],static_cast<int32>(Part.Tier)+1,Part.EnhanceLevel),PartIcon(static_cast<int32>(Part.Slot))));
            }
            if(GetVisibleOwnedCount()==0)Rows->AddChildToVerticalBox(Label(TEXT("보관 중인 파츠가 없습니다."),18));
            continue;
        }
        const auto Weapon=Column==0?EWeaponSlot::Pistol:EWeaponSlot::Rifle;Box->AddChildToVerticalBox(Label(Column==0?TEXT("권총"):TEXT("소총"),22));
        auto* WeaponImage=WidgetTree->ConstructWidget<UImage>();WeaponImage->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,Column==0?TEXT("/Game/UI/DreamVeilSkin/Icons/T_Icon_Pistol"):TEXT("/Game/UI/DreamVeilSkin/Icons/T_Icon_Rifle")));
        auto* ImageSize=WidgetTree->ConstructWidget<USizeBox>();ImageSize->SetHeightOverride(82);ImageSize->SetContent(WeaponImage);Box->AddChildToVerticalBox(ImageSize)->SetPadding(FMargin(12,8));
        if(auto* W=P->GetWeaponInSlot(Weapon))Box->AddChildToVerticalBox(Label(FString::Printf(TEXT("피해 %.0f  ·  연사 %.3f초\n사거리 %.0fm"),W->GetFinalDamage(),W->GetFireInterval(),W->GetRange()/100.f),18))->SetPadding(FMargin(0,4,0,12));
        else Box->AddChildToVerticalBox(Label(TEXT("미보유 무기"),18));
        for(int32 SlotIndex=0;SlotIndex<(Weapon==EWeaponSlot::Pistol?3:5);++SlotIndex)
        {
            int32 Index=INDEX_NONE;for(int32 I=0;I<Parts.Num();++I)if(Parts[I].bEquipped&&Parts[I].Weapon==Weapon&&static_cast<int32>(Parts[I].Slot)==SlotIndex){Index=I;break;}
            auto* B=Button(Box,TEXT(""),Index>=0?20000+Index:-2,Index>=0);
            const FString Text=Index>=0?FString::Printf(TEXT("%s · %d등급 +%d\n클릭하여 해제"),SlotNames[SlotIndex],static_cast<int32>(Parts[Index].Tier)+1,Parts[Index].EnhanceLevel):FString::Printf(TEXT("%s · 비어 있음"),SlotNames[SlotIndex]);
            B->SetContent(PartRow(Text,Index>=0?PartIcon(SlotIndex):nullptr));
        }
    }
    Status->SetText(FText::FromString(TEXT("인벤토리를 보는 동안 게임이 일시정지됩니다.  ·  I / Z / ESC 닫기")));
}
void UDreamInventoryWidget::HandleAction(int32 Action)
{
    if(Action==-1){Super::HandleAction(Action);return;}auto* P=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());if(!P||!P->Inventory)return;
    if(Action>=20000)P->Inventory->UnequipPart(Action-20000);else if(Action>=10000)P->Inventory->EquipPart(Action-10000);
    RefreshPanel();
}
