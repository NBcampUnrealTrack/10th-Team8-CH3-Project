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
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "MainPlayerController.h"
#include "MainGameModeBase.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "GameFramework/WorldSettings.h"
#include "MainMenuGameMode.h"
#include "Engine/World.h"
#include "AutomatedAssetImportData.h"
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
    //Parent 호출 노드가 없는 위젯도 있음 그때는 Event Construct 자체에 이어 붙임
    for (UEdGraph* Graph : BP->UbergraphPages)
        for (UEdGraphNode* Node : TArray<TObjectPtr<UEdGraphNode>>(Graph->Nodes))
            if (auto* Event = Cast<UK2Node_Event>(Node))
                if (Event->EventReference.GetMemberName() == TEXT("Construct"))
                {
                    auto* Fn = Call(Graph, Function);
                    auto* Out = Event->FindPin(UEdGraphSchema_K2::PN_Then);
                    const auto OldLinks = Out->LinkedTo; Out->BreakAllPinLinks();
                    Link(Out, Fn->GetExecPin());
                    for (auto* Pin : OldLinks) Link(Fn->GetThenPin(), Pin);
                    return;
                }

    bOK = false; UE_LOG(LogTemp, Error, TEXT("Missing Construct in %s"), *BP->GetName());
}

//self가 가진 함수나 커스텀 이벤트를 부르는 노드를 만듦
//위의 Call은 UI 라이브러리 전용(Widget 핀에 self를 꽂음)이라 따로 둠
UK2Node_CallFunction* CallSelf(UEdGraph* Graph, UClass* OwnerClass, FName FunctionName)
{
    UFunction* TargetFunction = OwnerClass ? OwnerClass->FindFunctionByName(FunctionName) : nullptr;

    if (!TargetFunction)
    {
        bOK = false;
        UE_LOG(LogTemp, Error, TEXT("Missing function %s on %s"), *FunctionName.ToString(), *GetNameSafe(OwnerClass));
        return nullptr;
    }

    UK2Node_CallFunction* Node = NewObject<UK2Node_CallFunction>(Graph);
    Node->SetFromFunction(TargetFunction);
    Graph->AddNode(Node, false, false);
    Node->CreateNewGuid();
    Node->AllocateDefaultPins();

    //자리는 아무 데나 겹치지 않게만 둠 기존 노드 밑으로 쌓음
    Node->NodePosX = 1000;
    Node->NodePosY = Graph->Nodes.Num() * 90;

    return Node;
}

//그 그래프에서 이 함수를 부르는 첫 노드
UK2Node_CallFunction* FindCall(UEdGraph* Graph, FName FunctionName)
{
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(Node);

        if (CallNode && CallNode->FunctionReference.GetMemberName() == FunctionName)
        {
            return CallNode;
        }
    }

    return nullptr;
}
}

//메인 메뉴 맵이 메인 메뉴 전용 게임모드를 쓰게 함
//이걸 해야 메뉴에서도 배경음 조절 키가 먹고 마우스 클릭도 그대로 됨
bool SetMainMenuGameMode()
{
    bOK = true;

    UWorld* MainMenuWorld = LoadObject<UWorld>(nullptr, TEXT("/Game/Maps/MainMenu.MainMenu"));

    if (!MainMenuWorld || !MainMenuWorld->GetWorldSettings())
    {
        UE_LOG(LogTemp, Error, TEXT("Missing MainMenu map or its world settings"));
        return false;
    }

    MainMenuWorld->GetWorldSettings()->DefaultGameMode = AMainMenuGameMode::StaticClass();

    //맵은 확장자가 umap이라 위의 Save를 쓰면 uasset으로 나감 여기서만 따로 저장함
    UPackage* Package = MainMenuWorld->GetOutermost();
    const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetMapPackageExtension());

    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;

    return UPackage::SavePackage(Package, MainMenuWorld, *Filename, Args);
}

//배경음 전용 사운드 클래스와 믹스를 만들고 BGM 다섯 곡을 그 클래스에 넣음
//이렇게 해두면 볼륨을 오디오 컴포넌트마다 걸지 않고 믹스 하나로 전체에 걸 수 있음
//맵이 바뀌어 새 컴포넌트가 생겨도 같은 클래스에 속하므로 볼륨을 다시 먹일 필요가 없음
bool SetupDreamVeilBGMMix()
{
    bOK = true;

    const TCHAR* const BGMFolder = TEXT("/Game/Audio/BGM");

    //이미 있으면 그대로 쓰고 없으면 만듦 두 번 돌려도 덮어쓰지 않음
    USoundClass* BGMSoundClass = LoadObject<USoundClass>(nullptr, TEXT("/Game/Audio/BGM/SC_BGM.SC_BGM"));

    if (!BGMSoundClass)
    {
        UPackage* Package = CreatePackage(*FString::Printf(TEXT("%s/SC_BGM"), BGMFolder));
        BGMSoundClass = NewObject<USoundClass>(Package, TEXT("SC_BGM"), RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(BGMSoundClass);

        if (!Save(BGMSoundClass))
        {
            return false;
        }
    }

    USoundMix* BGMSoundMix = LoadObject<USoundMix>(nullptr, TEXT("/Game/Audio/BGM/SMix_BGM.SMix_BGM"));

    if (!BGMSoundMix)
    {
        UPackage* Package = CreatePackage(*FString::Printf(TEXT("%s/SMix_BGM"), BGMFolder));
        BGMSoundMix = NewObject<USoundMix>(Package, TEXT("SMix_BGM"), RF_Public | RF_Standalone);

        //비어 있는 믹스로 둠 어느 클래스를 얼마로 줄일지는 게임에서 SetSoundMixClassOverride로 정함
        FAssetRegistryModule::AssetCreated(BGMSoundMix);

        if (!Save(BGMSoundMix))
        {
            return false;
        }
    }

    for (const TCHAR* WaveName : {TEXT("BGM_MainMenu"), TEXT("BGM_Lobby"), TEXT("BGM_Level"), TEXT("BGM_Boss"), TEXT("BGM_Endless")})
    {
        USoundWave* Wave = LoadObject<USoundWave>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), BGMFolder, WaveName, WaveName));

        if (!Wave)
        {
            bOK = false;
            UE_LOG(LogTemp, Error, TEXT("Missing BGM wave %s"), WaveName);
            continue;
        }

        //음소거는 소리를 0으로 만드는 방식이라 그동안에도 곡이 계속 흘러야 함
        //PlayWhenSilent가 아니면 안 들리는 동안 소리가 통째로 멈춰서 다시 켤 때 처음부터 나옴
        if (Wave->SoundClassObject == BGMSoundClass && Wave->VirtualizationMode == EVirtualizationMode::PlayWhenSilent)
        {
            continue;
        }

        Wave->SoundClassObject = BGMSoundClass;
        Wave->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;

        if (!Save(Wave))
        {
            bOK = false;
        }
    }

    return bOK;
}

//메인 메뉴 위젯이 열릴 때 메인 메뉴 배경음을 틀도록 연결함
//메인 메뉴 맵은 엔진 기본 게임모드를 써서 AMainGameModeBase가 돌지 않기 때문에 위젯이 대신 부름
bool WireMainMenuBGM()
{
    bOK = true;

    UWidgetBlueprint* MainMenu = WidgetBP(TEXT("WBP_MainMenu"));

    if (!MainMenu)
    {
        UE_LOG(LogTemp, Error, TEXT("Missing WBP_MainMenu"));
        return false;
    }

    //이미 연결돼 있으면 또 붙이지 않음 두 번 붙으면 곡이 두 겹으로 흐름
    for (UEdGraph* Graph : MainMenu->UbergraphPages)
    {
        if (FindCall(Graph, TEXT("PlayMainMenuBGM")))
        {
            UE_LOG(LogTemp, Display, TEXT("WBP_MainMenu already plays the main menu BGM"));
            return true;
        }
    }

    AppendToConstruct(MainMenu, TEXT("PlayMainMenuBGM"));

    if (!bOK || !Compile(MainMenu))
    {
        return false;
    }

    return Save(MainMenu);
}

//BGM wav 다섯 개를 들여오고 반복 재생을 켠 뒤 게임모드 기본값에 꽂음
//Looping을 여기서 켜는 이유 SpawnSound2D는 스스로 반복하지 않아서 이걸 안 켜면 한 번 울리고 조용해짐
//에디터에서 다섯 번 클릭할 일을 없애려고 임포트부터 배정까지 한 번에 함
bool ImportDreamVeilBGM(const FString& SourceDirectory)
{
    bOK = true;

    //파일 이름 그대로 에셋 이름이 되므로 미리 정해둔 이름으로 맞춰 옮겨둔 폴더를 받음
    struct FBGMEntry { const TCHAR* AssetName; const TCHAR* PropertyName; };

    const FBGMEntry Entries[] = {
        { TEXT("BGM_MainMenu"), TEXT("MainMenuBGM") },
        { TEXT("BGM_Lobby"),    TEXT("LobbyBGM")    },
        { TEXT("BGM_Level"),    TEXT("LevelBGM")    },
        { TEXT("BGM_Boss"),     TEXT("BossBGM")     },
        { TEXT("BGM_Endless"),  TEXT("EndlessBGM")  },
    };

    UAutomatedAssetImportData* ImportData = NewObject<UAutomatedAssetImportData>();
    ImportData->bReplaceExisting = true;
    ImportData->DestinationPath = TEXT("/Game/Audio/BGM");

    for (const FBGMEntry& Entry : Entries)
    {
        ImportData->Filenames.Add(SourceDirectory / FString::Printf(TEXT("%s.wav"), Entry.AssetName));
    }

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    const TArray<UObject*> Imported = AssetTools.ImportAssetsAutomated(ImportData);

    if (Imported.Num() != UE_ARRAY_COUNT(Entries))
    {
        UE_LOG(LogTemp, Error, TEXT("BGM import brought in %d of %d files"), Imported.Num(), (int32)UE_ARRAY_COUNT(Entries));
        return false;
    }

    for (UObject* Asset : Imported)
    {
        USoundWave* Wave = Cast<USoundWave>(Asset);

        if (!Wave)
        {
            bOK = false;
            continue;
        }

        //배경음은 끊기면 안 되므로 반복을 켬
        Wave->bLooping = true;

        if (!Save(Wave))
        {
            bOK = false;
        }
    }

    //게임모드 기본값에 꽂음 맵마다 게임모드가 새로 만들어져도 이 값은 클래스 기본값이라 그대로 따라감
    UBlueprint* GameModeBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprint/BP_MainGameModeBase.BP_MainGameModeBase"));

    if (!GameModeBP || !GameModeBP->GeneratedClass)
    {
        UE_LOG(LogTemp, Error, TEXT("Missing BP_MainGameModeBase"));
        return false;
    }

    UObject* GameModeDefaults = GameModeBP->GeneratedClass->GetDefaultObject();
    GameModeBP->Modify();

    for (const FBGMEntry& Entry : Entries)
    {
        USoundWave* Wave = LoadObject<USoundWave>(nullptr, *FString::Printf(TEXT("/Game/Audio/BGM/%s.%s"), Entry.AssetName, Entry.AssetName));

        //protected 멤버라 직접 대입하지 못하므로 리플렉션으로 넣음
        FObjectProperty* Property = FindFProperty<FObjectProperty>(GameModeDefaults->GetClass(), Entry.PropertyName);

        if (!Wave || !Property)
        {
            bOK = false;
            UE_LOG(LogTemp, Error, TEXT("Could not assign %s"), Entry.PropertyName);
            continue;
        }

        Property->SetObjectPropertyValue_InContainer(GameModeDefaults, Wave);
    }

    if (!bOK)
    {
        return false;
    }

    return Save(GameModeBP);
}

//상점과 인벤토리 블루프린트의 끊긴 연결을 고침
//1 상점을 열었을 때 판매 강화 목록이 비어 있던 문제
//2 메뉴를 한 번 열면 인벤토리가 다시 안 열리던 문제
bool FixDreamVeilShopAndInventory()
{
    bOK = true;

    // 1 상점 InitPlayer가 RefreshAll만 부르고 끝나서 판매 강화 목록을 채우지 않았음
    //   HandleInventoryChanged가 이미 목록까지 전부 다시 그리는 체인을 들고 있으므로 그걸 부르게 바꿈
    //   목록을 새로 그리는 노드를 또 만들지 않는 이유 같은 순서가 두 군데로 갈라지면 한쪽만 고쳐짐
    UWidgetBlueprint* Shop = WidgetBP(TEXT("WBP_Computer"));

    if (!Shop)
    {
        UE_LOG(LogTemp, Error, TEXT("Missing WBP_Computer"));
        return false;
    }

    bool bShopFixed = false;

    for (UEdGraph* Graph : Shop->UbergraphPages)
    {
        UK2Node_CallFunction* BindEventsCall = FindCall(Graph, TEXT("BindEvents"));

        if (!BindEventsCall)
        {
            continue;
        }

        UEdGraphPin* ThenPin = BindEventsCall->GetThenPin();

        //BindEvents 다음에 붙어 있던 노드가 정말 RefreshAll인지 확인하고 지움
        //이름으로만 찾으면 HandleInventoryChanged 쪽 RefreshAll을 지울 수 있음
        TArray<UEdGraphNode*> NodesToRemove;

        for (UEdGraphPin* Linked : ThenPin->LinkedTo)
        {
            UK2Node_CallFunction* LinkedCall = Cast<UK2Node_CallFunction>(Linked->GetOwningNode());

            if (LinkedCall && LinkedCall->FunctionReference.GetMemberName() == TEXT("RefreshAll"))
            {
                NodesToRemove.Add(LinkedCall);
            }
        }

        ThenPin->BreakAllPinLinks();

        for (UEdGraphNode* Node : NodesToRemove)
        {
            FBlueprintEditorUtils::RemoveNode(Shop, Node, true);
        }

        UK2Node_CallFunction* RefreshEverything = CallSelf(Graph, Shop->SkeletonGeneratedClass, TEXT("HandleInventoryChanged"));

        if (!RefreshEverything)
        {
            return false;
        }

        Link(ThenPin, RefreshEverything->GetExecPin());
        bShopFixed = true;
        break;
    }

    if (!bShopFixed)
    {
        bOK = false;
        UE_LOG(LogTemp, Error, TEXT("Could not find the BindEvents call in WBP_Computer"));
    }

    // 2 OpenInventory가 블루프린트 변수 bMenuOpen을 보고 있었음
    //   메뉴를 닫는 건 C++의 CloseMenuWidget이라 이 변수를 false로 내려줄 사람이 없어서
    //   컴퓨터든 침대든 한 번 열면 그 뒤로 인벤토리가 영영 안 열렸음
    //   이제 C++이 실제로 들고 있는 상태(IsMenuWidgetOpen)를 직접 물어봄
    UBlueprint* Controller = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprint/BP_MainPlayerController.BP_MainPlayerController"));

    if (!Controller)
    {
        UE_LOG(LogTemp, Error, TEXT("Missing BP_MainPlayerController"));
        return false;
    }

    bool bInventoryFixed = false;

    for (UEdGraph* Graph : Controller->FunctionGraphs)
    {
        if (Graph->GetFName() != TEXT("OpenInventory"))
        {
            continue;
        }

        for (UEdGraphNode* Node : TArray<TObjectPtr<UEdGraphNode>>(Graph->Nodes))
        {
            UK2Node_IfThenElse* Branch = Cast<UK2Node_IfThenElse>(Node);

            if (!Branch)
            {
                continue;
            }

            UEdGraphPin* ConditionPin = Branch->GetConditionPin();

            //조건에 물려 있던 bMenuOpen Get 노드는 쓸 데가 없어져서 같이 지움
            TArray<UEdGraphNode*> NodesToRemove;

            for (UEdGraphPin* Linked : ConditionPin->LinkedTo)
            {
                if (Cast<UK2Node_VariableGet>(Linked->GetOwningNode()))
                {
                    NodesToRemove.Add(Linked->GetOwningNode());
                }
            }

            ConditionPin->BreakAllPinLinks();

            for (UEdGraphNode* Dead : NodesToRemove)
            {
                FBlueprintEditorUtils::RemoveNode(Controller, Dead, true);
            }

            UK2Node_CallFunction* IsMenuOpen = CallSelf(Graph, AMainPlayerController::StaticClass(), TEXT("IsMenuWidgetOpen"));

            if (!IsMenuOpen)
            {
                return false;
            }

            Link(IsMenuOpen->GetReturnValuePin(), ConditionPin);
            bInventoryFixed = true;
            break;
        }

        break;
    }

    if (!bInventoryFixed)
    {
        bOK = false;
        UE_LOG(LogTemp, Error, TEXT("Could not find the Branch in BP_MainPlayerController::OpenInventory"));
    }

    if (!bOK)
    {
        return false;
    }

    //컴파일이 깨지면 저장하지 않음 반쯤 고쳐진 에셋을 남기지 않으려는 것
    if (!Compile(Shop) || !Compile(Controller))
    {
        return false;
    }

    return Save(Shop) && Save(Controller);
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
    const EAugmentID IDs[] = {EAugmentID::AttackUp, EAugmentID::DefenceUp, EAugmentID::HealthUp, EAugmentID::StaminaUp, EAugmentID::Berserker, EAugmentID::LastFortress, EAugmentID::ThornArmor, EAugmentID::Vampire, EAugmentID::Regeneration, EAugmentID::Knockback, EAugmentID::AreaAttack, EAugmentID::ContinuousAttack};
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
        for (int32 Pitch : {-50, 0, 50})
        {
            Arm->SetRelativeRotation(FRotator(Pitch, Yaw, 0));
            Arm->TickComponent(0.016f, LEVELTICK_All, nullptr);
            const FVector Camera = Arm->GetSocketLocation(USpringArmComponent::SocketName);
            const float Clearance = WallDistance - FVector::DotProduct(Camera, Axis) * Sign;
            if (Clearance < Arm->ProbeSize - 0.1f)
            {
                bPassed = false;
                UE_LOG(LogTemp, Warning, TEXT("CAMERA TEST FAIL: side=%d yaw=%d clearance=%.2f origin=%s camera=%s"), Side, Yaw, Clearance, *Arm->GetComponentLocation().ToString(), *Camera.ToString());
            }
        }
    }
    World->DestroyWorld(false);
    UE_LOG(LogTemp, Display, TEXT("CAMERA WALL TEST: %s (4 walls x 8 yaw angles x 3 pitch angles, capsule radius %.2f)."), bPassed ? TEXT("PASS") : TEXT("FAIL"), WallDistance - 2.0f);
    return bPassed;
}

bool UpdateHardCameraAssets(bool bApply)
{
    // Only touch the two requested UI assets, retaining all graph connections and styling.
    for (const TCHAR* Name : {TEXT("WBP_MainMenu"), TEXT("WBP_GameOver")})
    {
        auto* BP = WidgetBP(Name);
        if (!BP) return false;
        auto Rewrite = [&](const FText& Original) -> FText {
            FString Value = Original.ToString();
            if (Value.IsEmpty()) return Original;
            UE_LOG(LogTemp, Display, TEXT("HARD UI TEXT %s: %s"), Name, *Value);
            // These destination phrases describe the old Hardcore failure rule.
            Value.ReplaceInline(TEXT("메인 메뉴로 돌아간다"), TEXT("로비로 돌아간다"));
            Value.ReplaceInline(TEXT("메뉴로 돌아간다"), TEXT("로비로 돌아간다"));
            Value.ReplaceInline(TEXT("메인 메뉴로 돌아갑니다"), TEXT("로비로 돌아갑니다"));
            Value.ReplaceInline(TEXT("로비에 돌아올 때마다 자동으로 저장된다."), TEXT("로비 복귀 시 자동 저장됩니다. 단, 하드코어 게임 오버 직후에는 저장하지 않습니다."));
            Value.ReplaceInline(TEXT("시간이 다 된 경우 모두 꿈에서 깨어 로비 레벨로 돌아간다."), TEXT("시간 초과 시 로비로 돌아갑니다. 하드코어는 저장과 진행도도 초기화됩니다."));
            if (Value != Original.ToString())
            {
                UE_LOG(LogTemp, Display, TEXT("HARD UI REPLACE: %s"), *Value);
                return FText::FromString(Value);
            }
            return Original;
        };
        BP->WidgetTree->ForEachWidget([&](UWidget* W) {
            if (auto* Label = Cast<UTextBlock>(W))
            {
                const FText Updated = Rewrite(Label->GetText());
                if (bApply) Label->SetText(Updated);
            }
        });
        TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
        for (auto* Graph : Graphs)
            for (UEdGraphNode* Node : Graph->Nodes)
                for (auto* Pin : Node->Pins)
                {
                    if (!Pin->DefaultTextValue.IsEmpty())
                    {
                        const FText Updated = Rewrite(Pin->DefaultTextValue);
                        if (bApply) Pin->DefaultTextValue = Updated;
                    }
                    else if (!Pin->DefaultValue.IsEmpty() && (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_String))
                    {
                        const FText Updated = Rewrite(FText::FromString(Pin->DefaultValue));
                        if (bApply) Pin->DefaultValue = Updated.ToString();
                    }
                }
        if (bApply && (!Compile(BP) || !Save(BP))) return false;
    }
    auto* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprint/BP_MainPlayerCharacter.BP_MainPlayerCharacter"));
    auto* Player = BP && BP->GeneratedClass ? Cast<AMainPlayerCharacter>(BP->GeneratedClass->GetDefaultObject()) : nullptr;
    if (!Player) return false;
    if (bApply)
    {
        Player->ConfigureCameraCollision();
        if (!Compile(BP) || !Save(BP)) return false;
    }
    UE_LOG(LogTemp, Display, TEXT("HARD CAMERA ASSETS: %s"), bApply ? TEXT("SAVED") : TEXT("AUDITED"));
    return true;
}
