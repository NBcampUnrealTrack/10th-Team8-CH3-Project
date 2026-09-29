#include "DreamVeilSkinCommandlet.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Components/WidgetSwitcher.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/BorderSlot.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Overlay.h"
#include "Components/ButtonSlot.h"
#include "Components/ScrollBoxSlot.h"
#include "Brushes/SlateColorBrush.h"
#include "DVVisual.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"
#include "HAL/FileManager.h"

namespace DreamVeilSkin
{
using FJson = TSharedPtr<FJsonObject>;
const TCHAR* AssetRoot = TEXT("/Game/UI/DreamVeilSkin/");

FString Str(const FJson& Obj, const TCHAR* Key, const FString& Default = TEXT(""))
{
    FString Out;
    return Obj->TryGetStringField(Key, Out) ? Out : Default;
}
double Num(const FJson& Obj, const TCHAR* Key, double Default = 0)
{
    double Out;
    return Obj->TryGetNumberField(Key, Out) ? Out : Default;
}
FString GraphFingerprint(UWidgetBlueprint* BP)
{
    TArray<UEdGraph*> Graphs;
    BP->GetAllGraphs(Graphs);
    TArray<FString> Rows;
    for (UEdGraph* Graph : Graphs)
    {
        Rows.Add(Graph->GetName());
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node) continue;
            Rows.Add(Node->GetClass()->GetPathName() + Node->NodeGuid.ToString());
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin) continue;
                // Pins of reflected struct nodes can be regenerated at load. Compare
                // semantic endpoints and defaults rather than transient pin GUIDs.
                FString Row = Node->NodeGuid.ToString()+Pin->PinName.ToString()+FString::FromInt(Pin->Direction)+Pin->PinType.PinCategory.ToString()+Pin->DefaultValue+Pin->DefaultTextValue.ToString();
                if(Pin->DefaultObject) Row+=Pin->DefaultObject->GetPathName();
                TArray<FString> Links;
                for (UEdGraphPin* Link : Pin->LinkedTo) if (Link) Links.Add(Link->GetOwningNode()->NodeGuid.ToString()+Link->PinName.ToString());
                Links.Sort();
                Rows.Add(Row + FString::Join(Links, TEXT("|")));
            }
        }
    }
    Rows.Sort();
    return FMD5::HashAnsiString(*FString::Join(Rows, TEXT("\n")));
}
FJson Audit(UWidgetBlueprint* BP)
{
    FJson Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("asset"), BP->GetPathName());
    Result->SetStringField(TEXT("graph_fingerprint"), GraphFingerprint(BP));
    Result->SetNumberField(TEXT("binding_count"), BP->Bindings.Num());
    Result->SetNumberField(TEXT("animation_count"), BP->Animations.Num());
    TArray<TSharedPtr<FJsonValue>> Widgets;
    BP->WidgetTree->ForEachWidget([&](UWidget* W)
    {
        FJson Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("name"), W->GetName());
        Item->SetStringField(TEXT("class"), W->GetClass()->GetName());
        Item->SetStringField(TEXT("parent"), W->GetParent() ? W->GetParent()->GetName() : TEXT(""));
        if (UTextBlock* Text = Cast<UTextBlock>(W)) Item->SetStringField(TEXT("text"), Text->GetText().ToString());
        if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(W->Slot))
        {
            Item->SetNumberField(TEXT("x"), Slot->GetPosition().X);
            Item->SetNumberField(TEXT("y"), Slot->GetPosition().Y);
            Item->SetNumberField(TEXT("w"), Slot->GetSize().X);
            Item->SetNumberField(TEXT("h"), Slot->GetSize().Y);
        }
        Widgets.Add(MakeShared<FJsonValueObject>(Item));
    });
    Result->SetArrayField(TEXT("widgets"), Widgets);
    return Result;
}
FSlateBrush TextureBrush(const FString& Relative, float Margin = 0)
{
    FSlateBrush Brush;
    const FString Path = FString(AssetRoot) + Relative;
    UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Path);
    Brush.SetResourceObject(Texture);
    Brush.ImageSize = Texture ? FVector2D(Texture->GetSizeX(), Texture->GetSizeY()) : FVector2D(512,128);
    Brush.DrawAs = Margin > 0 ? ESlateBrushDrawType::Box : ESlateBrushDrawType::Image;
    Brush.Margin = FMargin(Margin);
    return Brush;
}
void StyleWidget(UWidget* W, double FontSize = 18)
{
    if (UTextBlock* T = Cast<UTextBlock>(W))
    {
        FSlateFontInfo Font = T->GetFont();
        Font.Size = FMath::RoundToInt(FontSize);
        Font.OutlineSettings.OutlineSize = 1;
        Font.OutlineSettings.OutlineColor = FLinearColor(0.01f,0.008f,0.012f,0.8f);
        T->SetFont(Font);
        T->SetColorAndOpacity(FSlateColor(FLinearColor::FromSRGBColor(FColor(242,238,231))));
        T->SetShadowOffset(FVector2D(0,1));
        T->SetShadowColorAndOpacity(FLinearColor(0,0,0,0.7f));
    }
    if (UButton* B = Cast<UButton>(W))
    {
        FButtonStyle Style = B->GetStyle();
        Style.Normal = TextureBrush(TEXT("Buttons/T_Button_Normal"),0.0625f);
        Style.Hovered = TextureBrush(TEXT("Buttons/T_Button_Hovered"),0.0625f);
        Style.Pressed = TextureBrush(TEXT("Buttons/T_Button_Pressed"),0.0625f);
        Style.Disabled = TextureBrush(TEXT("Buttons/T_Button_Disabled"),0.0625f);
        Style.NormalPadding = FMargin(16,8);
        Style.PressedPadding = FMargin(16,9,16,7);
        B->SetStyle(Style);
    }
    if (UProgressBar* P = Cast<UProgressBar>(W))
    {
        FProgressBarStyle Style = P->GetWidgetStyle();
        Style.BackgroundImage.TintColor = FSlateColor(FLinearColor(0.012f,0.01f,0.018f,0.86f));
        P->SetWidgetStyle(Style);
        if (P->GetName().Contains(TEXT("Health"))) P->SetFillColorAndOpacity(FLinearColor::FromSRGBColor(FColor(184,46,58)));
        if (P->GetName().Contains(TEXT("Stamina"))) P->SetFillColorAndOpacity(FLinearColor::FromSRGBColor(FColor(210,173,62)));
        if (P->GetName().Contains(TEXT("Experience"))) P->SetFillColorAndOpacity(FLinearColor::FromSRGBColor(FColor(201,205,208)));
    }
}
void FillSlot(UPanelSlot* Slot)
{
    if (auto* S=Cast<UButtonSlot>(Slot)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); S->SetPadding(FMargin(0)); }
    if (auto* S=Cast<UBorderSlot>(Slot)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); S->SetPadding(FMargin(0)); }
    if (auto* S=Cast<UVerticalBoxSlot>(Slot)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
    if (auto* S=Cast<UHorizontalBoxSlot>(Slot)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
    if (auto* S=Cast<UOverlaySlot>(Slot)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
    if (auto* S=Cast<USizeBoxSlot>(Slot)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
}
#include "NativeSkin.inl"
UCanvasPanel* ScopeCanvas(UWidgetTree* Tree, UWidget* Scope, FString& Error)
{
    if (auto* Canvas=Cast<UCanvasPanel>(Scope)) return Canvas;
    UPanelWidget* Panel=Cast<UPanelWidget>(Scope);
    if (!Panel || Cast<UWidgetSwitcher>(Scope) || Cast<UScrollBox>(Scope) || Cast<UButton>(Scope))
    {
        Error = TEXT("Cannot flatten interactive/switcher/scroll parent: ") + Scope->GetName();
        return nullptr;
    }
    FName Name(*FString::Printf(TEXT("Skin_Layout_%s"), *Scope->GetName()));
    if (auto* Existing=Tree->FindWidget<UCanvasPanel>(Name)) return Existing;
    TArray<UWidget*> Children=Panel->GetAllChildren();
    Panel->ClearChildren();
    UCanvasPanel* Canvas=Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),Name);
    FillSlot(Panel->AddChild(Canvas));
    for (UWidget* Child:Children)
    {
        UCanvasPanelSlot* Slot=Canvas->AddChildToCanvas(Child);
        Slot->SetAnchors(FAnchors(0,0,1,1)); Slot->SetOffsets(FMargin(0));
    }
    return Canvas;
}
bool ApplyScreen(UWidgetBlueprint* BP, const FJson& Screen, FString& Error)
{
    const TArray<TSharedPtr<FJsonValue>>* Entries;
    if (!Screen->TryGetArrayField(TEXT("widgets"),Entries)) return true;
    UWidgetTree* Tree=BP->WidgetTree;
    TMap<FString,FJson> Specs;
    for (const auto& Value:*Entries) { FJson E=Value->AsObject(); if(E.IsValid()) Specs.Add(Str(E,TEXT("name")),E); }
    FString PageName=Str(Screen,TEXT("page_root"));
    UWidget* Root=PageName.IsEmpty()?Tree->RootWidget.Get():Tree->FindWidget(FName(*PageName));
    if (!Root) { Error=TEXT("Missing page_root: ")+PageName; return false; }
    for (const auto& Value:*Entries)
    {
        FJson E=Value->AsObject();
        const FString Name=Str(E,TEXT("name"));
        if (Name.IsEmpty() || Name.StartsWith(TEXT("<"))) continue;
        const FString Role=Str(E,TEXT("role"),TEXT("existing"));
        UWidget* W=Tree->FindWidget(FName(*Name));
        if (!W && Role==TEXT("existing")) { Error=TEXT("Missing existing widget: ")+Name; return false; }
        if (!W)
        {
            FString Class=Str(E,TEXT("create_class"),TEXT("Image"));
            if (Class==TEXT("TextBlock"))
            {
                auto* T=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),FName(*Name));
                T->SetText(FText::FromString(Str(E,TEXT("text")))); W=T;
            }
            else W=Tree->ConstructWidget<UImage>(UImage::StaticClass(),FName(*Name));
            W->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        StyleWidget(W,Num(E,TEXT("font_size"),18));
        if (auto* Image=Cast<UImage>(W))
        {
            FString Relative=Str(E,TEXT("texture_relative_path"));
            if (!Relative.IsEmpty())
            {
                Relative=FPaths::ChangeExtension(Relative,TEXT(""));
                UTexture2D* Texture=LoadObject<UTexture2D>(nullptr,*(FString(AssetRoot)+Relative));
                if (!Texture) { Error=TEXT("Missing texture: ")+Relative; return false; }
                Image->SetBrushFromTexture(Texture,false);
            }
            const FString MaterialPath=Str(E,TEXT("material"));
            if (!MaterialPath.IsEmpty())
            {
                auto* Material=LoadObject<UMaterialInterface>(nullptr,*MaterialPath);
                if (!Material) { Error=TEXT("Missing material: ")+MaterialPath; return false; }
                Image->SetBrushFromMaterial(Material);
            }
        }
        const FString Policy=Str(E,TEXT("reparent_policy"));
        if (Policy==TEXT("preserve") && !Cast<UCanvasPanelSlot>(W->Slot)) continue;
        FString ParentName=Str(E,TEXT("parent"));
        UWidget* Parent=ParentName.IsEmpty()?Root:Tree->FindWidget(FName(*ParentName));
        if (!Parent) { Error=TEXT("Missing parent: ")+ParentName; return false; }
        if (Parent==W) continue;
        UCanvasPanel* Canvas=ScopeCanvas(Tree,Parent,Error);
        if (!Canvas) return false;
        if (W->GetParent()!=Canvas) { W->RemoveFromParent(); Canvas->AddChild(W); }
        auto* Slot=CastChecked<UCanvasPanelSlot>(W->Slot);
        double PX=0,PY=0;
        if (FJson* PS=Specs.Find(ParentName)) { PX=Num(*PS,TEXT("x")); PY=Num(*PS,TEXT("y")); }
        Slot->SetAnchors(FAnchors(0,0));
        Slot->SetAlignment(FVector2D(0,0));
        Slot->SetPosition(FVector2D(Num(E,TEXT("x"))-PX,Num(E,TEXT("y"))-PY));
        Slot->SetSize(FVector2D(Num(E,TEXT("w"),100),Num(E,TEXT("h"),40)));
        Slot->SetAutoSize(false);
        Slot->SetZOrder((int32)Num(E,TEXT("z_order"),Role==TEXT("new_visual")?-5:0));
    }
    return true;
}
}

UDreamVeilSkinCommandlet::UDreamVeilSkinCommandlet()
{
    IsClient=false; IsServer=false; IsEditor=true; LogToConsole=true;
}
int32 UDreamVeilSkinCommandlet::Main(const FString& Params)
{
    using namespace DreamVeilSkin;
    FString ManifestPath,ReportPath,BackupDirectory;
    FParse::Value(*Params,TEXT("Manifest="),ManifestPath);
    FParse::Value(*Params,TEXT("Report="),ReportPath);
    FParse::Value(*Params,TEXT("BackupDirectory="),BackupDirectory);
    const bool bApply=FParse::Param(*Params,TEXT("StageSkin"));
    // This delivery is audit-only until a hierarchy-specific application manifest
    // has been validated against the real project. Never apply the design spec.
    if (FParse::Param(*Params,TEXT("Apply")))
    {
        UE_LOG(LogTemp,Error,TEXT("Apply disabled in preview kit: real-project hierarchy audit and validated application manifest are required. No assets changed."));
        return 2;
    }
    if (bApply && FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()) != TEXT("C:/Users/2mir0/Documents/Codex/2026-09-22/new-chat/work/unreal/DreamVeilStage/"))
    { UE_LOG(LogTemp,Error,TEXT("StageSkin is restricted to the independent audited workspace copy.")); return 2; }
    if (ReportPath.IsEmpty()) ReportPath=FPaths::ProjectSavedDir()/TEXT("DreamVeilSkin/audit.json");
    if(bApply && (ManifestPath.IsEmpty() || BackupDirectory.IsEmpty()))
    {
        UE_LOG(LogTemp,Error,TEXT("Apply requires -Manifest and -BackupDirectory. Nothing saved.")); return 2;
    }
    FJson Manifest;
    if(!ManifestPath.IsEmpty())
    {
        FString Json;
        if(!FFileHelper::LoadFileToString(Json,*ManifestPath)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json),Manifest))
        { UE_LOG(LogTemp,Error,TEXT("Cannot read manifest.")); return 2; }
    }
    TArray<TSharedPtr<FJsonValue>> Reports;
    TArray<UWidgetBlueprint*> Blueprints;
    const TArray<FString> Names={TEXT("WBP_HUD"),TEXT("WBP_Inventory"),TEXT("WBP_PartEntry"),TEXT("WBP_Computer"),TEXT("WBP_AugmentChoice"),TEXT("WBP_Bed"),TEXT("WBP_GameOver"),TEXT("WBP_MainMenu")};
    TMap<UWidgetBlueprint*,FString> Fingerprints;
    int32 Failures=0;
    for(const FString& Name:Names)
    {
        const FString Path=TEXT("/Game/UI/")+Name+TEXT(".")+Name;
        UWidgetBlueprint* BP=LoadObject<UWidgetBlueprint>(nullptr,*Path);
        if(!BP||!BP->WidgetTree) { UE_LOG(LogTemp,Error,TEXT("Missing widget blueprint: %s"),*Path); ++Failures; continue; }
        Blueprints.Add(BP); Fingerprints.Add(BP,GraphFingerprint(BP));
        Reports.Add(MakeShared<FJsonValueObject>(Audit(BP)));
    }
    FJson Report=MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("mode"),bApply?TEXT("apply"):TEXT("audit_only"));
    Report->SetArrayField(TEXT("before"),Reports);
    if(bApply && Failures==0)
    {
        const TArray<TSharedPtr<FJsonValue>>* Baseline;
        if(!Manifest->TryGetArrayField(TEXT("before"),Baseline)) { UE_LOG(LogTemp,Error,TEXT("An original audit is required.")); return 2; }
        for (UWidgetBlueprint* BP:Blueprints)
        {
            bool Found=false;
            for (auto& Item:*Baseline) if(Str(Item->AsObject(),TEXT("asset"))==BP->GetPathName())
            { Found=Str(Item->AsObject(),TEXT("graph_fingerprint"))==Fingerprints[BP]; break; }
            if(!Found || BP->WidgetTree->FindWidget(TEXT("DVI_RootScale")) || BP->WidgetTree->FindWidget(TEXT("DVI_RowSize")))
            { UE_LOG(LogTemp,Error,TEXT("Baseline mismatch or skin already applied: %s"),*BP->GetName()); return 2; }
        }
        // Copy every original widget file before touching loaded widget objects.
        for(UWidgetBlueprint* BP:Blueprints)
        {
            const FString Source=FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
            const FString Backup=FPaths::ConvertRelativePathToFull(BackupDirectory/BP->GetName()+TEXT(".uasset"));
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup),true);
            if(IFileManager::Get().FileExists(*Backup)||IFileManager::Get().Copy(*Backup,*Source,false)!=COPY_OK)
            { UE_LOG(LogTemp,Error,TEXT("Backup not created (existing backups never overwritten): %s"),*Backup); return 3; }
        }
        for(UWidgetBlueprint* BP:Blueprints)
        {
            BP->Modify();
            TArray<UWidget*> OriginalWidgets;
            BP->WidgetTree->GetAllWidgets(OriginalWidgets);
            FNativeSkin Skin(BP); Skin.Run();
            TArray<UWidget*> AfterWidgets;
            BP->WidgetTree->GetAllWidgets(AfterWidgets);
            for(UWidget* W:OriginalWidgets) if(!AfterWidgets.Contains(W))
            { UE_LOG(LogTemp,Error,TEXT("Original widget detached: %s/%s"),*BP->GetName(),*W->GetName()); ++Failures; }
            if(GraphFingerprint(BP)!=Fingerprints[BP]) { UE_LOG(LogTemp,Error,TEXT("Graph identity changed: %s"),*BP->GetName()); ++Failures; }
            if(Failures) break;
            for(UWidget* W:AfterWidgets) if(!BP->WidgetVariableNameToGuidMap.Contains(W->GetFName())) BP->OnVariableAdded(W->GetFName());
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
            FCompilerResultsLog CompileLog;
            FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipGarbageCollection,&CompileLog);
            if(CompileLog.NumErrors>0) { UE_LOG(LogTemp,Error,TEXT("Compile errors: %s"),*BP->GetName()); ++Failures; }
            if(GraphFingerprint(BP)!=Fingerprints[BP]) { UE_LOG(LogTemp,Error,TEXT("Graph identity changed after compile: %s"),*BP->GetName()); ++Failures; }
        }
        if(Failures==0)
        {
            for(UWidgetBlueprint* BP:Blueprints)
            {
                UPackage* Package=BP->GetOutermost();
                const FString Filename=FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension());
                FSavePackageArgs SaveArgs; SaveArgs.TopLevelFlags=RF_Public|RF_Standalone;
                if(!UPackage::SavePackage(Package,BP,*Filename,SaveArgs)) ++Failures;
            }
        }
        TArray<TSharedPtr<FJsonValue>> After;
        for(UWidgetBlueprint* BP:Blueprints) After.Add(MakeShared<FJsonValueObject>(Audit(BP)));
        Report->SetArrayField(TEXT("after"),After);
    }
    Report->SetNumberField(TEXT("failure_count"),Failures);
    Report->SetBoolField(TEXT("saved"),bApply&&Failures==0);
    FString Json;
    FJsonSerializer::Serialize(Report.ToSharedRef(),TJsonWriterFactory<TCHAR,TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath),true);
    if(!FFileHelper::SaveStringToFile(Json,*ReportPath)) return 4;
    UE_LOG(LogTemp,Display,TEXT("DreamVeil skin %s. Failures: %d. Report: %s"),bApply?TEXT("apply"):TEXT("audit only"),Failures,*ReportPath);
    return Failures?1:0;
}
