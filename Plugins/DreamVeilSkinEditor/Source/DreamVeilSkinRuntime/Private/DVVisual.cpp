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
bool PureNumber(UObject* Object,const TCHAR* Name,double& Out) {
 if(!Object)return false;auto* Fn=Object->FindFunction(Name);
 if(!Fn||!Fn->HasAnyFunctionFlags(FUNC_BlueprintPure)||Fn->NumParms!=1)return false;
 FStructOnScope Args(Fn);Object->ProcessEvent(Fn,Args.GetStructMemory());
 auto* Return=Fn->GetReturnProperty();return Return&&Number(Return,Return->ContainerPtrToValuePtr<void>(Args.GetStructMemory()),Out);
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
 TArray<UActorComponent*> Components;Pawn->GetComponents(Components);
 for(auto* Component:Components) {
  auto* Fn=Component->FindFunction(TEXT("GetParts"));if(!Fn||!Fn->HasAnyFunctionFlags(FUNC_BlueprintPure)||Fn->NumParms!=1)continue;
  auto* Array=CastField<FArrayProperty>(Fn->GetReturnProperty());auto* Inner=Array?CastField<FStructProperty>(Array->Inner):nullptr;if(!Inner)continue;
  FStructOnScope Args(Fn);Component->ProcessEvent(Fn,Args.GetStructMemory());
  FScriptArrayHelper Helper(Array,Array->ContainerPtrToValuePtr<void>(Args.GetStructMemory()));
  if(Mode==TEXT("EquippedPart")) {
   for(int32 I=0;I<Helper.Num();I++) {
    const void* Data=Helper.GetRawPtr(I);double Weapon=0,S=0;
    auto* Equipped=FindFProperty<FBoolProperty>(Inner->Struct,TEXT("bEquipped"));
    if(Equipped&&Equipped->GetPropertyValue_InContainer(Data)&&FieldNumber(Inner->Struct,Data,TEXT("Weapon"),Weapon)&&FieldNumber(Inner->Struct,Data,TEXT("Slot"),S)&&(int32)Weapon==WeaponIndex&&(int32)S==PartSlot)
     return PartFields(Inner->Struct,Data,PartSlot,Tier);
   }
   return false;
  }
  const int32 Index=(int32)IndexValue;if(!Helper.IsValidIndex(Index))return false;
  return PartFields(Inner->Struct,Helper.GetRawPtr(Index),PartSlot,Tier);
 }
 return false;
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
void UDVVisual::NativeTick(const FGeometry& Geometry,float DeltaTime){Super::NativeTick(Geometry,DeltaTime);Elapsed+=DeltaTime;if(Elapsed>=.10f){Elapsed=0;RefreshVisual();}}
void UDVVisual::RefreshVisual() {
 if(Mode==TEXT("Timer")) {
  if(!TimerText)return;double Seconds=180;
  const bool Valid=IsDesignTime()||PureNumber(UGameplayStatics::GetGameMode(this),TEXT("GetLevelTimeRemaining"),Seconds);
  if(!Valid||Seconds<0){TimerText->SetText(FText::GetEmpty());return;}
  const int32 S=FMath::Max(0,FMath::CeilToInt(Seconds));TimerText->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"),S/60,S%60)));return;
 }
 if(!Icon||!TierFrame)return;
 int32 PartSlot=PreviewSlot,Tier=PreviewTier;bool Valid=true;
 if(!IsDesignTime()) {
  if(Mode==TEXT("Weapon")){double V=0;Valid=PureNumber(GetOwningPlayerPawn(),TEXT("GetCurrentWeaponSlot"),V);PartSlot=(int32)V;Valid&=PartSlot>=0&&PartSlot<2;}
  else if(Mode!=TEXT("Fixed")){Valid=GetPart(GetTypedOuter<UUserWidget>(),GetOwningPlayerPawn(),Mode,PartSlot,Tier,WeaponIndex);}
 }
 Icon->SetVisibility(Valid?ESlateVisibility::HitTestInvisible:ESlateVisibility::Hidden);
 TierFrame->SetVisibility(Valid&&bShowTier?ESlateVisibility::HitTestInvisible:ESlateVisibility::Hidden);
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
