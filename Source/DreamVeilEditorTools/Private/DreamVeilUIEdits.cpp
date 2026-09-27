#include "DreamVeilUIEdits.h"
#include "DreamUIBlueprintLibrary.h"
#include "BedActor.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_Self.h"
#include "EdGraphSchema_K2.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "MainPlayerCharacter.h"
#include "DreamVeilGameInstance.h"
#include "DispatchTableComponent.h"
#include "Slate/WidgetRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "Serialization/BufferArchive.h"
#include "Misc/FileHelper.h"
#include "ShaderCompiler.h"
#include "AssetCompilingManager.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"

namespace
{
bool bOK = true;
bool Save(UObject* Asset)
{
    if (!Asset) return false;
    auto* Package = Asset->GetOutermost();
    const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(Package, Asset, *Filename, Args);
}
bool Compile(UBlueprint* BP)
{
    if (auto* Widget = Cast<UWidgetBlueprint>(BP))
    {
        TSet<FName> Present;
        Widget->WidgetTree->ForEachWidget([&](UWidget* W) {
            Present.Add(W->GetFName());
            if (W->bIsVariable && !Widget->WidgetVariableNameToGuidMap.Contains(W->GetFName()))
                Widget->OnVariableAdded(W->GetFName());
        });
        for (auto It = Widget->WidgetVariableNameToGuidMap.CreateIterator(); It; ++It)
            if (!Present.Contains(It.Key())) It.RemoveCurrent();
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    if (BP->Status == BS_Error) { UE_LOG(LogTemp, Error, TEXT("UI update compile failed: %s"), *BP->GetPathName()); return false; }
    return true;
}
void Link(UEdGraphPin* A, UEdGraphPin* B)
{
    if (!A || !B || !GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A, B))
    {
        UE_LOG(LogTemp, Error, TEXT("UI update failed to link %s -> %s"), A ? *A->PinName.ToString() : TEXT("null"), B ? *B->PinName.ToString() : TEXT("null"));
        bOK = false;
    }
}
UK2Node_CallFunction* Call(UEdGraph* Graph, FName Function)
{
    auto* Node = NewObject<UK2Node_CallFunction>(Graph);
    Node->SetFromFunction(UDreamUIBlueprintLibrary::StaticClass()->FindFunctionByName(Function));
    Graph->AddNode(Node, false, false); Node->CreateNewGuid(); Node->AllocateDefaultPins();
    Node->NodePosX = 600; Node->NodePosY = Graph->Nodes.Num() * 100;
    auto* Self = NewObject<UK2Node_Self>(Graph);
    Graph->AddNode(Self, false, false); Self->CreateNewGuid(); Self->AllocateDefaultPins();
    Self->NodePosX = Node->NodePosX - 200; Self->NodePosY = Node->NodePosY + 80;
    Link(Self->FindPin(UEdGraphSchema_K2::PN_Self), Node->FindPin(TEXT("Widget")));
    return Node;
}
void SetRect(UWidget* Widget, FVector2D Pos, FVector2D Size)
{
    if (auto* Slot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
    { Slot->SetPosition(Pos); Slot->SetSize(Size); Slot->SetAutoSize(false); }
    else { bOK = false; UE_LOG(LogTemp, Error, TEXT("Missing canvas slot: %s"), *GetNameSafe(Widget)); }
}
UWidgetBlueprint* WidgetBP(const TCHAR* Name)
{
    return LoadObject<UWidgetBlueprint>(nullptr, *FString::Printf(TEXT("/Game/UI/%s.%s"), Name, Name));
}
void Text(UWidgetBlueprint* BP, const TCHAR* Name, const TCHAR* Caption)
{
    if (auto* T = Cast<UTextBlock>(BP->WidgetTree->FindWidget(Name))) T->SetText(FText::FromString(Caption));
    else bOK = false;
}
// Keep existing bound events, replace only their execution targets.
void ReplaceClick(UWidgetBlueprint* BP, FName Button, FName Function)
{
    for (UEdGraph* Graph : BP->UbergraphPages)
        for (UEdGraphNode* Node : TArray<TObjectPtr<UEdGraphNode>>(Graph->Nodes))
            if (auto* Event = Cast<UK2Node_ComponentBoundEvent>(Node))
                if (Event->ComponentPropertyName == Button && Event->DelegatePropertyName == TEXT("OnClicked"))
                {
                    auto* Fn = Call(Graph, Function);
                    auto* Out = Event->FindPin(UEdGraphSchema_K2::PN_Then);
                    Out->BreakAllPinLinks(); Link(Out, Fn->GetExecPin()); return;
                }
    bOK = false; UE_LOG(LogTemp, Error, TEXT("Missing click event %s"), *Button.ToString());
}
void AddClick(UWidgetBlueprint* BP, UEdGraph* Graph, FName Button, FName Function, int32 Level = -1)
{
    const auto* Property = FindFProperty<FObjectProperty>(BP->SkeletonGeneratedClass, Button);
    const auto* Delegate = FindFProperty<FMulticastDelegateProperty>(UButton::StaticClass(), TEXT("OnClicked"));
    if (!Property || !Delegate) { bOK = false; return; }
    auto* Event = NewObject<UK2Node_ComponentBoundEvent>(Graph);
    Event->InitializeComponentBoundEventParams(Property, Delegate);
    Graph->AddNode(Event, false, false); Event->CreateNewGuid(); Event->AllocateDefaultPins();
    auto* Fn = Call(Graph, Function);
    if (Level >= 0) Fn->FindPinChecked(TEXT("LevelNumber"))->DefaultValue = FString::FromInt(Level);
    Event->NodePosY = Fn->NodePosY;
    Link(Event->FindPin(UEdGraphSchema_K2::PN_Then), Fn->GetExecPin());
}
void AppendToConstruct(UWidgetBlueprint* BP, FName Function)
{
    for (UEdGraph* Graph : BP->UbergraphPages)
        for (UEdGraphNode* Node : TArray<TObjectPtr<UEdGraphNode>>(Graph->Nodes))
            if (auto* Parent = Cast<UK2Node_CallParentFunction>(Node))
                if (Parent->FunctionReference.GetMemberName() == TEXT("Construct"))
                {
                    auto* Fn = Call(Graph, Function);
                    auto* Out = Parent->GetThenPin();
                    const auto OldLinks = Out->LinkedTo; Out->BreakAllPinLinks();
                    Link(Out, Fn->GetExecPin());
                    for (auto* Pin : OldLinks) Link(Fn->GetThenPin(), Pin);
                    return;
                }
    bOK = false; UE_LOG(LogTemp, Error, TEXT("Missing parent Construct in %s"), *BP->GetName());
}
}

bool ApplyDreamVeilUIEdits(const FString& IconDirectory)
{
    bOK = true;
    auto* Bed = WidgetBP(TEXT("WBP_Bed"));
    auto* Augment = WidgetBP(TEXT("WBP_AugmentChoice"));
    auto* GameOver = WidgetBP(TEXT("WBP_GameOver"));
    auto* BedActorBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Maps/BP/Object/BP_BedActor.BP_BedActor"));
    if (!Bed || !Augment || !GameOver || !BedActorBP) return false;
    if (FPackageName::DoesPackageExist(TEXT("/Game/UI/WBP_DreamSelect")))
    { UE_LOG(LogTemp, Error, TEXT("WBP_DreamSelect already exists; refusing to overwrite. Use audit/verify instead.")); return false; }

    const TCHAR* Files[] = {TEXT("01_bullet"), TEXT("02_shield_up"), TEXT("03_healing"), TEXT("04_speed_boot"), TEXT("05_berserk"), TEXT("06_fortress"), TEXT("07_thorns"), TEXT("08_vampire"), TEXT("09_regeneration"), TEXT("10_sticky_ground"), TEXT("11_target_three_monsters"), TEXT("12_fireball")};
    const EAugmentID IDs[] = {EAugmentID::AttackUp, EAugmentID::DefenceUp, EAugmentID::HealthUp, EAugmentID::StaminaUp, EAugmentID::Berserker, EAugmentID::LastFortress, EAugmentID::ThornArmor, EAugmentID::Vampire, EAugmentID::Regeneration, EAugmentID::SlowEnemy, EAugmentID::AreaAttack, EAugmentID::ContinuousAttack};
    for (const TCHAR* File : Files)
        if (!FPaths::FileExists(IconDirectory / (FString(File) + TEXT(".png")))) return false;
    auto* IconPackage = CreatePackage(TEXT("/Game/UI/AugmentIcons/DA_AugmentIcons"));
    auto* IconSet = NewObject<UDreamAugmentIconSet>(IconPackage, TEXT("DA_AugmentIcons"), RF_Public | RF_Standalone);
    TArray<UObject*> Assets;
    for (int32 I = 0; I < 12; ++I)
    {
        const FString Name = FString(TEXT("T_Augment_")) + Files[I];
        auto* Pkg = CreatePackage(*(TEXT("/Game/UI/AugmentIcons/") + Name));
        auto* Factory = NewObject<UTextureFactory>();
        Factory->SuppressImportOverwriteDialog();
        auto* Texture = Cast<UTexture2D>(UFactory::StaticImportObject(UTexture2D::StaticClass(), Pkg, *Name, RF_Public | RF_Standalone, *(IconDirectory / (FString(Files[I]) + TEXT(".png"))), nullptr, Factory));
        if (!Texture) return false;
        Texture->LODGroup = TEXTUREGROUP_UI; Texture->CompressionSettings = TC_EditorIcon;
        Texture->MipGenSettings = TMGS_NoMipmaps; Texture->SRGB = true; Texture->NeverStream = true;
        Texture->PostEditChange();
        IconSet->Icons.Add(IDs[I], Texture); Assets.Add(Texture);
    }
    Assets.Add(IconSet);

    // Duplicate the existing approved frame/mist/font design, not a new visual theme.
    auto& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    auto* Select = Cast<UWidgetBlueprint>(AssetTools.DuplicateAsset(TEXT("WBP_DreamSelect"), TEXT("/Game/UI"), Bed));
    if (!Select) return false;
    auto* Layout = Cast<UCanvasPanel>(Select->WidgetTree->FindWidget(TEXT("DVI_Layout_VB_Main")));
    auto* Go = Cast<UButton>(Select->WidgetTree->FindWidget(TEXT("Button_Go")));
    auto* GoText = Cast<UTextBlock>(Select->WidgetTree->FindWidget(TEXT("Text_Go")));
    if (!Layout || !Go || !GoText) return false;
    const FButtonStyle ButtonStyle = Go->GetStyle(); const FSlateFontInfo Font = GoText->GetFont();
    Go->RemoveFromParent(); Go->bIsVariable = false; GoText->bIsVariable = false;
    Select->OnVariableRemoved(TEXT("Button_Go"));
    Select->OnVariableRemoved(TEXT("Text_Go"));
    // Remove only copied click event nodes; the inherited Construct and animations remain.
    for (UEdGraph* G : Select->UbergraphPages)
        for (UEdGraphNode* N : TArray<TObjectPtr<UEdGraphNode>>(G->Nodes))
            if (Cast<UK2Node_ComponentBoundEvent>(N)) FBlueprintEditorUtils::RemoveNode(Select, N, true);
    for (const TCHAR* Name : {TEXT("VB_Main"), TEXT("DVI_BedPanelFill"), TEXT("DVI_BedPanelFrame")})
        SetRect(Select->WidgetTree->FindWidget(Name), {328,137}, {624,446});
    SetRect(Select->WidgetTree->FindWidget(TEXT("DVI_BedMist")), {340,149}, {600,422});
    Text(Select, TEXT("DVI_BedHeading"), TEXT("어떤 꿈에 들어가겠습니까?"));
    SetRect(Select->WidgetTree->FindWidget(TEXT("DVI_BedHeading")), {24,28}, {576,46});
    Text(Select, TEXT("Text_Info"), TEXT("클리어한 꿈도 다시 들어갈 수 있습니다."));
    SetRect(Select->WidgetTree->FindWidget(TEXT("Text_Info")), {24,84}, {576,40});
    Text(Select, TEXT("Text_Cancel"), TEXT("닫기"));
    SetRect(Select->WidgetTree->FindWidget(TEXT("Button_Cancel")), {212,370}, {200,42});
    for (int32 I = 1; I <= 5; ++I)
    {
        auto* B = Select->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("Button_Level%d"), I));
        B->bIsVariable = true; B->SetStyle(ButtonStyle); Layout->AddChild(B);
        auto* T = Select->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("Text_Level%d"), I));
        T->bIsVariable = true; T->SetFont(Font); T->SetJustification(ETextJustify::Center);
        T->SetText(FText::FromString(I == 5 ? TEXT("Endless") : FString::Printf(TEXT("L%d"), I)));
        T->SetVisibility(ESlateVisibility::HitTestInvisible); B->AddChild(T);
        SetRect(B, I == 5 ? FVector2D(34,268) : FVector2D(34 + ((I-1)%2)*296, 140 + ((I-1)/2)*64), I == 5 ? FVector2D(556,50) : FVector2D(260,50));
        CastChecked<UCanvasPanelSlot>(B->Slot)->SetZOrder(10);
    }
    if (!Compile(Select)) return false;
    UEdGraph* SelectGraph = Select->UbergraphPages[0];
    for (int32 I = 1; I <= 5; ++I) AddClick(Select, SelectGraph, *FString::Printf(TEXT("Button_Level%d"), I), TEXT("EnterDream"), I == 5 ? 0 : I);
    AddClick(Select, SelectGraph, TEXT("Button_Cancel"), TEXT("CloseBedDreamMenu"));
    AppendToConstruct(Select, TEXT("RefreshDreamSelection"));

    Text(Bed, TEXT("DVI_BedHeading"), TEXT("꿈에 들어가겠습니까?"));
    Text(Bed, TEXT("Text_Info"), TEXT("들어갈 꿈을 선택합니다."));
    Text(Bed, TEXT("Text_Go"), TEXT("예"));
    ReplaceClick(Bed, TEXT("Button_Go"), TEXT("OpenBedDreamSelect"));
    ReplaceClick(Bed, TEXT("Button_Cancel"), TEXT("CloseBedDreamMenu"));

    // Preserve the name/description/choice ID graphs. Only add icons to Setup.
    for (int32 I = 1; I <= 3; ++I)
    {
        auto* Panel = Cast<UCanvasPanel>(Augment->WidgetTree->FindWidget(*FString::Printf(TEXT("DVI_Layout_VB_Choice%d"), I)));
        if (!Panel) return false;
        auto* Image = Augment->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), *FString::Printf(TEXT("Image_Choice%dIcon"), I));
        Image->bIsVariable = true; Image->SetVisibility(ESlateVisibility::HitTestInvisible);
        // A square brush preserves the provided icon's aspect and transparent frame.
        Image->SetBrushFromTexture(IconSet->Icons[IDs[I-1]], false); Panel->AddChild(Image);
        SetRect(Image, {108,55}, {80,80}); CastChecked<UCanvasPanelSlot>(Image->Slot)->SetZOrder(10);
        auto* Name = Augment->WidgetTree->FindWidget(*FString::Printf(TEXT("Text_Choice%dName"), I));
        SetRect(Name, {24,136}, {248,36});
        if (auto* T = Cast<UTextBlock>(Name)) { auto NameFont = T->GetFont(); NameFont.Size = 21; T->SetFont(NameFont); T->SetJustification(ETextJustify::Center); }
    }
    UEdGraph* Setup = nullptr;
    for (UEdGraph* G : Augment->FunctionGraphs) if (G->GetFName() == TEXT("Setup")) Setup = G;
    if (!Setup) return false;
    UK2Node_FunctionEntry* Entry = nullptr;
    for (UEdGraphNode* N : Setup->Nodes) if (auto* E = Cast<UK2Node_FunctionEntry>(N)) Entry = E;
    if (!Entry) return false;
    auto* ApplyIcons = Call(Setup, TEXT("ApplyAugmentIcons"));
    ApplyIcons->FindPinChecked(TEXT("IconSet"))->DefaultObject = IconSet;
    Link(Entry->FindPin(TEXT("Choices")), ApplyIcons->FindPin(TEXT("Choices")));
    auto* Out = Entry->FindPinChecked(UEdGraphSchema_K2::PN_Then);
    auto OldLinks = Out->LinkedTo; Out->BreakAllPinLinks(); Link(Out, ApplyIcons->GetExecPin());
    for (auto* Pin : OldLinks) Link(ApplyIcons->GetThenPin(), Pin);

    ReplaceClick(GameOver, TEXT("Button_Continue"), TEXT("ContinueDreamGameOver"));
    if (!bOK || !Compile(Bed) || !Compile(Select) || !Compile(Augment) || !Compile(GameOver)) return false;

    UObject* BedDefaults = BedActorBP->GeneratedClass->GetDefaultObject();
    auto* ConfirmProperty = FindFProperty<FClassProperty>(ABedActor::StaticClass(), TEXT("ConfirmWidgetClass"));
    auto* SelectProperty = FindFProperty<FClassProperty>(ABedActor::StaticClass(), TEXT("DreamSelectWidgetClass"));
    if (!ConfirmProperty || !SelectProperty) return false;
    ConfirmProperty->SetObjectPropertyValue_InContainer(BedDefaults, Bed->GeneratedClass);
    SelectProperty->SetObjectPropertyValue_InContainer(BedDefaults, Select->GeneratedClass);
    // Native BedActor now owns opening the menu; disconnect the obsolete BP menu-opening event.
    // Preserve its graph for inspection instead of deleting user nodes.
    for (UEdGraph* G : BedActorBP->UbergraphPages)
        for (UEdGraphNode* N : G->Nodes)
            if (auto* E = Cast<UK2Node_Event>(N))
                if (E->EventReference.GetMemberName() == TEXT("OnBedInteracted"))
                    E->FindPinChecked(UEdGraphSchema_K2::PN_Then)->BreakAllPinLinks();
    if (!Compile(BedActorBP)) return false;
    Assets.Append({Bed, Select, Augment, GameOver, BedActorBP});
    for (UObject* Asset : Assets) { Asset->MarkPackageDirty(); if (!Save(Asset)) return false; }
    UE_LOG(LogTemp, Display, TEXT("Dream UI update saved %d assets. No materials or level lighting modified."), Assets.Num());
    return true;
}

bool VerifyDreamVeilUIEdits(bool bFinalize)
{
    auto* Select = WidgetBP(TEXT("WBP_DreamSelect"));
    auto* Bed = WidgetBP(TEXT("WBP_Bed"));
    auto* Augment = WidgetBP(TEXT("WBP_AugmentChoice"));
    auto* GameOver = WidgetBP(TEXT("WBP_GameOver"));
    auto* BedActor = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Maps/BP/Object/BP_BedActor.BP_BedActor"));
    auto* Player = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprint/BP_MainPlayerCharacter.BP_MainPlayerCharacter"));
    auto* Icons = LoadObject<UDreamAugmentIconSet>(nullptr, TEXT("/Game/UI/AugmentIcons/DA_AugmentIcons.DA_AugmentIcons"));
    if (!Select || !Bed || !Augment || !GameOver || !BedActor || !Player || !Icons || Icons->Icons.Num() != 12) return false;
    // Repair the variable GUID cache of the newly duplicated widget, then compile every changed BP.
    if (bFinalize)
    {
        if (!Compile(Select) || !Save(Select)) return false;
        auto* Character = Cast<AMainPlayerCharacter>(Player->GeneratedClass->GetDefaultObject());
        if (!Character) return false;
        auto* Arm = Character->FindComponentByClass<USpringArmComponent>();
        auto* Camera = Character->FindComponentByClass<UCameraComponent>();
        if (!Arm || !Camera) return false;
        Arm->SocketOffset += Camera->GetRelativeLocation(); Camera->SetRelativeLocation(FVector::ZeroVector);
        Arm->ProbeSize = FMath::Max(Arm->ProbeSize, 20.0f); Arm->ProbeChannel = ECC_Camera; Arm->bDoCollisionTest = true;
        if (!Compile(Player) || !Save(Player)) return false;
    }
    for (UBlueprint* BP : TArray<UBlueprint*>{Select, Bed, Augment, GameOver, BedActor, Player})
        if (!Compile(BP)) return false;
    for (auto& Pair : Icons->Icons) if (!Pair.Value) return false;
    for (int32 I = 1; I <= 3; ++I)
        if (!Augment->WidgetTree->FindWidget(*FString::Printf(TEXT("Image_Choice%dIcon"), I))) return false;
    int32 Clicks = 0;
    for (UEdGraph* G : Select->UbergraphPages)
        for (UEdGraphNode* N : G->Nodes)
            if (auto* E = Cast<UK2Node_ComponentBoundEvent>(N))
                if (E->DelegatePropertyName == TEXT("OnClicked") && E->FindPinChecked(UEdGraphSchema_K2::PN_Then)->LinkedTo.Num() == 1) ++Clicks;
    if (Clicks != 6) return false;
    auto* ConfirmProp = FindFProperty<FClassProperty>(ABedActor::StaticClass(), TEXT("ConfirmWidgetClass"));
    auto* SelectProp = FindFProperty<FClassProperty>(ABedActor::StaticClass(), TEXT("DreamSelectWidgetClass"));
    if (ConfirmProp->GetObjectPropertyValue_InContainer(BedActor->GeneratedClass->GetDefaultObject()) != Bed->GeneratedClass ||
        SelectProp->GetObjectPropertyValue_InContainer(BedActor->GeneratedClass->GetDefaultObject()) != Select->GeneratedClass) return false;
    UE_LOG(LogTemp, Display, TEXT("UI VERIFY PASS: 6 blueprints compile, 12 icons, 3 image slots, 6 selector click routes, BedActor class assignments."));
    return true;
}

bool RenderDreamVeilUI()
{
    // Isolated, unsaved world: never load or write the user's save-game slot.
    if (!FSlateApplication::IsInitialized())
    {
        UE_LOG(LogTemp, Error, TEXT("Render verification needs -AllowCommandletRendering without -NullRHI."));
        return false;
    }
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    auto* GI = NewObject<UDreamVeilGameInstance>(World);
    World->SetGameInstance(GI);
    auto* Icons = LoadObject<UDreamAugmentIconSet>(nullptr, TEXT("/Game/UI/AugmentIcons/DA_AugmentIcons.DA_AugmentIcons"));
    bool bPassed = Icons && Icons->Icons.Num() == 12 && GI->IsLevelUnlocked(1) && !GI->IsLevelUnlocked(2)
        && !GI->IsLevelUnlocked(0) && !GI->IsLevelUnlocked(5) && !GI->IsEndlessUnlocked() && !GI->IsLevelCleared(1);
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("UIUpdatePreviews");
    IFileManager::Get().MakeDirectory(*Directory, true);
    for (const TCHAR* Name : {TEXT("WBP_Bed"), TEXT("WBP_DreamSelect"), TEXT("WBP_AugmentChoice"), TEXT("WBP_GameOver")})
    {
        auto* BP = WidgetBP(Name);
        if (!BP || !BP->GeneratedClass) { bPassed = false; break; }
        auto* Widget = NewObject<UUserWidget>(World, BP->GeneratedClass);
        Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);
        if (!Widget->Initialize()) { bPassed = false; break; }
        Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);
        if (FString(Name) == TEXT("WBP_DreamSelect"))
        {
            UDreamUIBlueprintLibrary::RefreshDreamSelection(Widget);
            for (int32 I = 1; I <= 5; ++I)
            {
                auto* Button = Cast<UButton>(Widget->GetWidgetFromName(*FString::Printf(TEXT("Button_Level%d"), I)));
                bPassed &= Button && Button->GetIsEnabled() == (I == 1);
            }
        }
        if (FString(Name) == TEXT("WBP_AugmentChoice"))
        {
            UFunction* Setup = Widget->FindFunction(TEXT("Setup"));
            if (!Setup || !Icons) { bPassed = false; break; }
            // Exercise the actual BP Setup graph, not only the C++ icon helper.
            for (int32 Batch = 0; Batch < 4; ++Batch)
            {
                struct FSetupParams { TArray<EAugmentID> Choices; } Params;
                for (int32 I = 0; I < 3; ++I) Params.Choices.Add(static_cast<EAugmentID>(Batch * 3 + I));
                Widget->ProcessEvent(Setup, &Params);
                for (int32 I = 0; I < 3; ++I)
                {
                    auto* Icon = Cast<UImage>(Widget->GetWidgetFromName(*FString::Printf(TEXT("Image_Choice%dIcon"), I + 1)));
                    auto* Label = Cast<UTextBlock>(Widget->GetWidgetFromName(*FString::Printf(TEXT("Text_Choice%dName"), I + 1)));
                    const auto* Expected = Icons->Icons.Find(Params.Choices[I]);
                    bPassed &= Icon && Expected && Icon->GetBrush().GetResourceObject() == Expected->Get();
                    bPassed &= Label && Label->GetText().EqualTo(UDispatchTableComponent::GetAugmentDisplayName(Params.Choices[I]));
                }
            }
        }
        const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
        FAssetCompilingManager::Get().FinishAllCompilation();
        if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
        auto* Renderer = new FWidgetRenderer(true);
        auto* Target = FWidgetRenderer::CreateTargetFor(FVector2D(1280, 720), TF_Bilinear, true);
        Renderer->DrawWidget(Target, SlateWidget, FVector2D(1280, 720), 0.016f);
        FlushRenderingCommands();
        FBufferArchive PNG;
        const FString Filename = Directory / (FString(Name) + TEXT(".png"));
        const bool bExported = FImageUtils::ExportRenderTarget2DAsPNG(Target, PNG) && FFileHelper::SaveArrayToFile(PNG, *Filename);
        bPassed &= bExported;
        UE_LOG(LogTemp, Display, TEXT("UI PREVIEW %s: %s"), Name, bExported ? *Filename : TEXT("FAILED"));
        BeginCleanup(Renderer);
        Widget->ReleaseSlateResources(true);
    }
    World->DestroyWorld(false);
    UE_LOG(LogTemp, Display, TEXT("UI RENDER/DATA TEST: %s (initial unlocks, 12 live icon/name mappings, four previews)."), bPassed ? TEXT("PASS") : TEXT("FAIL"));
    return bPassed;
}

bool TestDreamVeilCamera()
{
    auto* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprint/BP_MainPlayerCharacter.BP_MainPlayerCharacter"));
    auto* Player = BP && BP->GeneratedClass ? Cast<AMainPlayerCharacter>(BP->GeneratedClass->GetDefaultObject()) : nullptr;
    auto* Defaults = Player ? Player->FindComponentByClass<USpringArmComponent>() : nullptr;
    if (!Defaults) return false;
    auto* World = UWorld::CreateWorld(EWorldType::Game, false);
    auto* Rig = World->SpawnActor<AActor>();
    auto* Root = NewObject<USceneComponent>(Rig); Rig->SetRootComponent(Root); Root->RegisterComponent();
    auto* Arm = NewObject<USpringArmComponent>(Rig);
    Arm->SetupAttachment(Root); Arm->SetRelativeLocation(Defaults->GetRelativeLocation());
    Arm->TargetArmLength = Defaults->TargetArmLength; Arm->SocketOffset = Defaults->SocketOffset;
    Arm->TargetOffset = Defaults->TargetOffset; Arm->ProbeSize = Defaults->ProbeSize;
    Arm->ProbeChannel = ECC_Camera; Arm->bDoCollisionTest = true; Arm->RegisterComponent();
    auto* Wall = World->SpawnActor<AActor>();
    auto* Box = NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box);
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionResponseToAllChannels(ECR_Ignore); Box->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
    Box->RegisterComponent();
    const float WallDistance = Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + 2.0f;
    bool bPassed = true;
    for (int32 Side = 0; Side < 4; ++Side)
    {
        const FVector Axis = Side < 2 ? FVector(1,0,0) : FVector(0,1,0);
        const float Sign = Side % 2 == 0 ? 1.0f : -1.0f;
        Box->SetBoxExtent(Side < 2 ? FVector(100,1000,1000) : FVector(1000,100,1000));
        Box->SetWorldLocation(Axis * Sign * (WallDistance + 100.0f));
        for (int32 Yaw = 0; Yaw < 360; Yaw += 45)
        {
            Arm->SetRelativeRotation(FRotator(0, Yaw, 0));
            Arm->TickComponent(0.016f, LEVELTICK_All, nullptr);
            const FVector Camera = Arm->GetSocketLocation(USpringArmComponent::SocketName);
            const float Clearance = WallDistance - FVector::DotProduct(Camera, Axis) * Sign;
            if (Clearance < 1.0f)
            {
                bPassed = false;
                UE_LOG(LogTemp, Warning, TEXT("CAMERA TEST FAIL: side=%d yaw=%d clearance=%.2f origin=%s camera=%s"), Side, Yaw, Clearance, *Arm->GetComponentLocation().ToString(), *Camera.ToString());
            }
        }
    }
    World->DestroyWorld(false);
    UE_LOG(LogTemp, Display, TEXT("CAMERA WALL TEST: %s (4 walls x 8 view angles, capsule radius %.2f)."), bPassed ? TEXT("PASS") : TEXT("FAIL"), WallDistance - 2.0f);
    return bPassed;
}
