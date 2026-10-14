#include "DVVisual.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/GameModeBase.h"
#include "Components/ActorComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"
#include "Modules/ModuleManager.h"
#include "InventoryComponent.h"
#include "MainPlayerCharacter.h"
#include "MainGameModeBase.h"
#include "DreamVeilGameInstance.h"
IMPLEMENT_MODULE(FDefaultModuleImpl,DreamVeilSkinRuntime)

namespace {
bool Number(FProperty* Prop,const void* Value,double& Out) {
 if(auto* E=CastField<FEnumProperty>(Prop)) {Out=E->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value);return true;}
 if(auto* N=CastField<FNumericProperty>(Prop)) {Out=N->IsFloatingPoint()?N->GetFloatingPointPropertyValue(Value):N->GetSignedIntPropertyValue(Value);return true;}
 return false;
}
bool FieldNumber(UStruct* Struct,const void* Data,const TCHAR* Name,double& Out) {
 auto* P=FindFProperty<FProperty>(Struct,Name);return P&&Number(P,P->ContainerPtrToValuePtr<void>(Data),Out);
}
bool PartFields(UScriptStruct* Type,const void* Data,int32& Slot,int32& Tier) {
 double S=0,T=0;if(!FieldNumber(Type,Data,TEXT("Slot"),S)||!FieldNumber(Type,Data,TEXT("Tier"),T))return false;
 Slot=(int32)S;Tier=(int32)T;return Slot>=0&&Slot<5;
}
bool GetPart(UUserWidget* Host,APawn* Pawn,FName Mode,int32& PartSlot,int32& Tier,int32 WeaponIndex) {
 if(!Host)return false;
 if(Mode==TEXT("Part")) {
  auto* P=FindFProperty<FStructProperty>(Host->GetClass(),TEXT("Part"));
  return P&&PartFields(P->Struct,P->ContainerPtrToValuePtr<void>(Host),PartSlot,Tier);
 }
 double IndexValue=-1;if(!Pawn)return false;
 if(Mode==TEXT("SelectedPart")&&(!FieldNumber(Host->GetClass(),Host,TEXT("SelectedPartIndex"),IndexValue)||IndexValue<0))return false;
 const UInventoryComponent* Inventory=Pawn->FindComponentByClass<UInventoryComponent>();
 if(!Inventory)return false;
 const TArray<FWeaponPart>& Parts=Inventory->GetPartsView();
 if(Mode==TEXT("EquippedPart")) {
  for(const FWeaponPart& Part:Parts)if(Part.bEquipped&&(int32)Part.Weapon==WeaponIndex&&(int32)Part.Slot==PartSlot) {PartSlot=(int32)Part.Slot;Tier=(int32)Part.Tier;return true;}
  return false;
 }
 const int32 Index=(int32)IndexValue;
 if(!Parts.IsValidIndex(Index))return false;
 PartSlot=(int32)Parts[Index].Slot;Tier=(int32)Parts[Index].Tier;return true;
}
}

TSharedRef<SWidget> UDVVisual::RebuildWidget() {
 if(!WidgetTree->RootWidget) {
  if(Mode==TEXT("Timer")) {
   TimerText=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("ReadOnlyTimer"));
   auto Font=TimerText->GetFont();Font.Size=TimerFontSize;Font.OutlineSettings.OutlineSize=1;TimerText->SetFont(Font);
   TimerText->SetColorAndOpacity(FSlateColor(FLinearColor::FromSRGBColor(FColor(242,238,231))));
   TimerText->SetJustification(ETextJustify::Center);WidgetTree->RootWidget=TimerText;
  } else {
   auto* Overlay=WidgetTree->ConstructWidget<UOverlay>();WidgetTree->RootWidget=Overlay;
   TierFrame=WidgetTree->ConstructWidget<UImage>();Icon=WidgetTree->ConstructWidget<UImage>();
   auto* FS=Overlay->AddChildToOverlay(TierFrame);FS->SetHorizontalAlignment(HAlign_Fill);FS->SetVerticalAlignment(VAlign_Fill);
   auto* IS=Overlay->AddChildToOverlay(Icon);IS->SetHorizontalAlignment(HAlign_Fill);IS->SetVerticalAlignment(VAlign_Fill);IS->SetPadding(FMargin(bShowTier?5:0));
   auto* Texture=LoadObject<UTexture2D>(nullptr,TEXT("/Game/UI/DreamVeilSkin/Frames/T_Frame_Item_9Slice"));
   TierFrame->SetBrushFromTexture(Texture);auto Brush=TierFrame->GetBrush();Brush.DrawAs=ESlateBrushDrawType::Box;Brush.Margin=FMargin(.0625f);TierFrame->SetBrush(Brush);
  }
 }
 return Super::RebuildWidget();
}
void UDVVisual::NativePreConstruct(){Super::NativePreConstruct();RefreshVisual();}
void UDVVisual::NativeConstruct(){Super::NativeConstruct();BindVisualEvents();RefreshVisual();}
void UDVVisual::NativeDestruct(){
 if(CachedInventory.IsValid())CachedInventory->OnInventoryChanged.RemoveDynamic(this,&UDVVisual::HandleInventoryChanged);
 if(CachedPlayer.IsValid())CachedPlayer->OnWeaponChanged.RemoveDynamic(this,&UDVVisual::HandleWeaponChanged);
 CachedInventory.Reset();CachedPlayer.Reset();Super::NativeDestruct();
}
void UDVVisual::BindVisualEvents(){
 if(IsDesignTime() || (Mode!=TEXT("EquippedPart") && Mode!=TEXT("Weapon")))return;
 AMainPlayerCharacter* Player=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());
 if(Player==CachedPlayer.Get()&&CachedInventory.IsValid())return;
 if(CachedInventory.IsValid())CachedInventory->OnInventoryChanged.RemoveDynamic(this,&UDVVisual::HandleInventoryChanged);
 if(CachedPlayer.IsValid())CachedPlayer->OnWeaponChanged.RemoveDynamic(this,&UDVVisual::HandleWeaponChanged);
 CachedPlayer=Player;CachedInventory=Player?Player->FindComponentByClass<UInventoryComponent>():nullptr;
 if(CachedInventory.IsValid())CachedInventory->OnInventoryChanged.AddUniqueDynamic(this,&UDVVisual::HandleInventoryChanged);
 if(Player)Player->OnWeaponChanged.AddUniqueDynamic(this,&UDVVisual::HandleWeaponChanged);
}
void UDVVisual::HandleInventoryChanged(){RefreshVisual();}
void UDVVisual::HandleWeaponChanged(EWeaponSlot NewSlot){(void)NewSlot;RefreshVisual();}
void UDVVisual::NativeTick(const FGeometry& Geometry,float DeltaTime){
 Super::NativeTick(Geometry,DeltaTime);
 if(Mode==TEXT("Fixed"))return;
 Elapsed+=DeltaTime;if(Elapsed<.10f)return;Elapsed=0;
 const bool bPawnChanged=GetOwningPlayerPawn()!=CachedPlayer.Get();
 if(bPawnChanged||!CachedInventory.IsValid())BindVisualEvents();
 if(!bPawnChanged&&CachedInventory.IsValid()&&(Mode==TEXT("EquippedPart")||Mode==TEXT("Weapon")))return;
 RefreshVisual();
}
void UDVVisual::RefreshVisual() {
 if(Mode==TEXT("Timer")) {
  if(!TimerText)return;
  const AMainGameModeBase* GameMode=Cast<AMainGameModeBase>(UGameplayStatics::GetGameMode(this));
  const UDreamVeilGameInstance* GI=GetGameInstance<UDreamVeilGameInstance>();
  const bool bEndless=GI&&GI->IsInEndless();
  const double Seconds=IsDesignTime()?180.0:(bEndless&&GetWorld()?GetWorld()->GetTimeSeconds():(GameMode?GameMode->GetLevelTimeRemaining():-1.0));
  const int32 S=Seconds<0?-1:FMath::Max(0,bEndless?FMath::FloorToInt(Seconds):FMath::CeilToInt(Seconds));
  if(S!=LastTimerSecond){TimerText->SetText(S<0?FText::GetEmpty():FText::FromString(FString::Printf(TEXT("%02d:%02d"),S/60,S%60)));LastTimerSecond=S;}
  return;
 }
 if(!Icon||!TierFrame)return;
 int32 PartSlot=PreviewSlot,Tier=PreviewTier;bool Valid=true;
 if(!IsDesignTime()) {
  if(Mode==TEXT("Weapon")){const AMainPlayerCharacter* Player=Cast<AMainPlayerCharacter>(GetOwningPlayerPawn());Valid=Player!=nullptr;PartSlot=Player?(int32)Player->GetCurrentWeaponSlot():-1;Valid&=PartSlot>=0&&PartSlot<2;}
  else if(Mode!=TEXT("Fixed")){Valid=GetPart(GetTypedOuter<UUserWidget>(),GetOwningPlayerPawn(),Mode,PartSlot,Tier,WeaponIndex);}
 }
 const ESlateVisibility IconVisibility=Valid?ESlateVisibility::HitTestInvisible:ESlateVisibility::Hidden;
 const ESlateVisibility FrameVisibility=Valid&&bShowTier?ESlateVisibility::HitTestInvisible:ESlateVisibility::Hidden;
 if(Icon->GetVisibility()!=IconVisibility)Icon->SetVisibility(IconVisibility);
 if(TierFrame->GetVisibility()!=FrameVisibility)TierFrame->SetVisibility(FrameVisibility);
 if(!Valid)return;
 if(PartSlot!=LastSlot) {
  static const TCHAR* Names[]={TEXT("Muzzle"),TEXT("Magazine"),TEXT("Sight"),TEXT("Stock"),TEXT("Foregrip")};
  FString Name=Mode==TEXT("Weapon")?(PartSlot==0?TEXT("Pistol"):TEXT("Rifle")):Names[FMath::Clamp(PartSlot,0,4)];
  Icon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,*(FString(TEXT("/Game/UI/DreamVeilSkin/Icons/T_Icon_"))+Name)));LastSlot=PartSlot;
 }
 if(Tier!=LastTier) {
  static const FColor Colors[]={FColor(101,199,232),FColor(155,102,231),FColor(226,183,63),FColor(82,199,118),FColor(202,202,208)};
  TierFrame->SetColorAndOpacity(FLinearColor::FromSRGBColor(Colors[FMath::Clamp(Tier,0,4)]));LastTier=Tier;
 }
}
