#include "DreamVeilUIUpdateCommandlet.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Blueprint.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "DreamVeilUIEdits.h"
#include "FileHelpers.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"

UDreamVeilUIUpdateCommandlet::UDreamVeilUIUpdateCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}

//블루프린트 하나의 위젯 트리와 모든 그래프를 글로 뽑음
//BP 그래프의 연결 상태를 코드에서 확인할 방법이 이것뿐이라 진단용으로 둠 에셋은 건드리지 않음
static FString DumpBlueprintGraphs(const FString& ObjectPath)
{
    UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *ObjectPath);

    if (!BP)
    {
        return FString::Printf(TEXT("\nERROR could not load %s\n"), *ObjectPath);
    }

    FString Report = FString::Printf(TEXT("\n===== ASSET %s PARENT %s\n"), *BP->GetName(), *GetPathNameSafe(BP->ParentClass));

    //위젯 BP만 위젯 트리를 가짐 컨트롤러 BP 같은 일반 BP는 이 블록을 건너뜀
    if (UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(BP))
    {
        WidgetBP->WidgetTree->ForEachWidget([&Report](UWidget* Widget)
        {
            Report += FString::Printf(TEXT("W %s %s parent=%s var=%d"),
                *Widget->GetName(), *Widget->GetClass()->GetName(), *GetNameSafe(Widget->GetParent()), Widget->bIsVariable ? 1 : 0);

            if (UTextBlock* TextWidget = Cast<UTextBlock>(Widget))
            {
                Report += TEXT(" text=") + TextWidget->GetText().ToString();
            }

            Report += TEXT("\n");
        });
    }

    //변수 목록도 같이 뽑음 어느 변수가 상태를 들고 있는지 봐야 연결을 판단할 수 있음
    for (const FBPVariableDescription& Variable : BP->NewVariables)
    {
        Report += FString::Printf(TEXT("VAR %s : %s\n"), *Variable.VarName.ToString(), *Variable.VarType.PinCategory.ToString());
    }

    //클래스 기본값에 꽂힌 에셋도 같이 찍음 어느 칸에 무엇이 들어갔는지 확인할 방법이 필요함
    if (UObject* Defaults = BP->GeneratedClass ? BP->GeneratedClass->GetDefaultObject() : nullptr)
    {
        for (TFieldIterator<FObjectProperty> PropertyIterator(Defaults->GetClass()); PropertyIterator; ++PropertyIterator)
        {
            UObject* Value = PropertyIterator->GetObjectPropertyValue_InContainer(Defaults);

            if (Value)
            {
                Report += FString::Printf(TEXT("DEFAULT %s = %s\n"), *PropertyIterator->GetName(), *Value->GetName());
            }
        }
    }

    TArray<UEdGraph*> Graphs;
    BP->GetAllGraphs(Graphs);

    for (UEdGraph* Graph : Graphs)
    {
        Report += TEXT("GRAPH ") + Graph->GetName() + TEXT("\n");

        for (UEdGraphNode* Node : Graph->Nodes)
        {
            Report += FString::Printf(TEXT("NODE %s | %s | %s\n"),
                *Node->GetName(), *Node->GetClass()->GetName(),
                *Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Replace(TEXT("\n"), TEXT(" / ")));

            for (UEdGraphPin* Pin : Node->Pins)
            {
                //연결이 없고 기본값도 없는 핀은 줄만 늘리므로 건너뜀
                if (Pin->LinkedTo.Num() == 0 && Pin->DefaultValue.IsEmpty() && Pin->DefaultObject == nullptr && Pin->DefaultTextValue.IsEmpty())
                {
                    continue;
                }

                Report += FString::Printf(TEXT("   %s %s default=%s%s ->"),
                    Pin->Direction == EGPD_Input ? TEXT("IN ") : TEXT("OUT"),
                    *Pin->PinName.ToString(),
                    *Pin->DefaultValue,
                    Pin->DefaultObject ? *(TEXT(" obj=") + GetNameSafe(Pin->DefaultObject)) : TEXT(""));

                for (UEdGraphPin* Linked : Pin->LinkedTo)
                {
                    Report += TEXT(" ") + Linked->GetOwningNode()->GetName() + TEXT(".") + Linked->PinName.ToString();
                }

                Report += TEXT("\n");
            }
        }
    }

    return Report;
}

int32 UDreamVeilUIUpdateCommandlet::Main(const FString& Params)
{
    if (FParse::Param(*Params, TEXT("FixShop"))) return FixDreamVeilShopAndInventory() ? 0 : 21;
    if (FParse::Param(*Params, TEXT("WireMenuBGM"))) return WireMainMenuBGM() ? 0 : 23;
    if (FParse::Param(*Params, TEXT("BGMMix"))) return SetupDreamVeilBGMMix() ? 0 : 24;
    if (FParse::Param(*Params, TEXT("MenuGameMode"))) return SetMainMenuGameMode() ? 0 : 25;

    FString BGMDirectory;

    if (FParse::Value(*Params, TEXT("ImportBGM="), BGMDirectory, false))
    {
        return ImportDreamVeilBGM(BGMDirectory) ? 0 : 22;
    }

    FString DumpList;

    //예) -DumpUI=/Game/UI/WBP_Computer.WBP_Computer,/Game/Blueprint/BP_MainPlayerController.BP_MainPlayerController
    //마지막 false가 없으면 FParse가 콤마에서 값을 끊어서 첫 에셋만 읽음
    if (FParse::Value(*Params, TEXT("DumpUI="), DumpList, false))
    {
        TArray<FString> ObjectPaths;
        DumpList.ParseIntoArray(ObjectPaths, TEXT(","));

        FString Dump;

        for (const FString& ObjectPath : ObjectPaths)
        {
            Dump += DumpBlueprintGraphs(ObjectPath);
        }

        //한글 주석과 UI 문구가 섞여 있어서 UTF-8로 강제 저장함 안 그러면 UTF-16으로 나가서 읽기 번거로움
        return FFileHelper::SaveStringToFile(Dump, *(FPaths::ProjectSavedDir() / TEXT("DreamVeilUIDump.txt")),
            FFileHelper::EEncodingOptions::ForceUTF8) ? 0 : 20;
    }

    if (FParse::Param(*Params, TEXT("HardCameraAudit")) || FParse::Param(*Params, TEXT("HardCameraApply")))
        return UpdateHardCameraAssets(FParse::Param(*Params, TEXT("HardCameraApply"))) ? 0 : 17;
    if (FParse::Param(*Params, TEXT("Render"))) return RenderDreamVeilUI() ? 0 : 15;
    if (FParse::Param(*Params, TEXT("TestCamera"))) return TestDreamVeilCamera() ? 0 : 16;
    if (FParse::Param(*Params, TEXT("Finalize")) || FParse::Param(*Params, TEXT("Verify")))
        if (!VerifyDreamVeilUIEdits(FParse::Param(*Params, TEXT("Finalize")))) return 11;
    if (FParse::Param(*Params, TEXT("Apply")))
    {
        FString Icons;
        if (!FParse::Value(*Params, TEXT("Icons="), Icons) || !ApplyDreamVeilUIEdits(Icons)) return 10;
    }
    FString Report;
    for (const TCHAR* Name : {TEXT("WBP_MenuBase"), TEXT("WBP_Bed"), TEXT("WBP_AugmentChoice"), TEXT("WBP_Corruption"), TEXT("WBP_GameOver")})
    {
        UWidgetBlueprint* BP = LoadObject<UWidgetBlueprint>(nullptr, *FString::Printf(TEXT("/Game/UI/%s.%s"), Name, Name));
        if (!BP) return 1;
        Report += FString::Printf(TEXT("\nASSET %s PARENT %s\n"), Name, *GetPathNameSafe(BP->ParentClass));
        BP->WidgetTree->ForEachWidget([&](UWidget* W) {
            Report += FString::Printf(TEXT("W %s %s parent=%s"), *W->GetName(), *W->GetClass()->GetName(), *GetNameSafe(W->GetParent()));
            if (auto* T = Cast<UTextBlock>(W)) Report += TEXT(" text=") + T->GetText().ToString();
            if (auto* S = Cast<UCanvasPanelSlot>(W->Slot)) Report += FString::Printf(TEXT(" xy=%s size=%s"), *S->GetPosition().ToString(), *S->GetSize().ToString());
            Report += TEXT("\n");
        });
        TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
        for (auto* G : Graphs) {
            Report += TEXT("GRAPH ") + G->GetName() + TEXT("\n");
            for (UEdGraphNode* N : G->Nodes) {
                Report += FString::Printf(TEXT("NODE %s %s %s\n"), *N->GetName(), *N->GetClass()->GetName(), *N->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
                for (auto* P : N->Pins) {
                    Report += FString::Printf(TEXT("  %s[%d] default=%s %s %s ->"), *P->PinName.ToString(), (int)P->Direction, *P->DefaultValue, *P->DefaultTextValue.ToString(), *GetPathNameSafe(P->DefaultObject));
                    for (auto* L : P->LinkedTo) Report += TEXT(" ") + L->GetOwningNode()->GetName() + TEXT(".") + L->PinName.ToString();
                    Report += TEXT("\n");
                }
            }
        }
    }
    auto* PlayerBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/Blueprint/BP_MainPlayerCharacter.BP_MainPlayerCharacter"));
    if (!PlayerBP || !PlayerBP->GeneratedClass)
    {
        Report += TEXT("ERROR: BP_MainPlayerCharacter could not be loaded; no assets were modified.\n");
        FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("DreamVeilUIAudit.txt")));
        return 2;
    }
    auto* Actor = Cast<AActor>(PlayerBP->GeneratedClass->GetDefaultObject());
    TArray<UActorComponent*> Components; Actor->GetComponents(Components);
    for (auto* C : Components) {
        if (auto* S = Cast<USpringArmComponent>(C)) Report += FString::Printf(TEXT("SPRING %s loc=%s rot=%s length=%f socket=%s target=%s test=%d probe=%f channel=%d\n"), *S->GetName(), *S->GetRelativeLocation().ToString(), *S->GetRelativeRotation().ToString(), S->TargetArmLength, *S->SocketOffset.ToString(), *S->TargetOffset.ToString(), S->bDoCollisionTest, S->ProbeSize, (int)S->ProbeChannel);
        if (auto* C2 = Cast<UCameraComponent>(C)) Report += FString::Printf(TEXT("CAMERA %s loc=%s rot=%s\n"), *C2->GetName(), *C2->GetRelativeLocation().ToString(), *C2->GetRelativeRotation().ToString());
    }
    if (FParse::Param(*Params, TEXT("AuditMaps")))
    {
        for (const TCHAR* Map : {TEXT("Lobby"), TEXT("LV_1"), TEXT("LV_2"), TEXT("LV_3"), TEXT("LV_4"), TEXT("EndlessLV")})
        {
            auto* World = UEditorLoadingAndSavingUtils::LoadMap(FString::Printf(TEXT("/Game/Maps/Level/%s"), Map));
            if (!World) return 12;
            Report += FString::Printf(TEXT("MAP %s\n"), Map);
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                TArray<UStaticMeshComponent*> Meshes; It->GetComponents(Meshes);
                for (auto* Mesh : Meshes)
                {
                    UStaticMesh* Asset = Mesh->GetStaticMesh();
                    if (!Asset || !Asset->GetName().Contains(TEXT("Wall"))) continue;
                    auto* Body = Asset->GetBodySetup();
                    Report += FString::Printf(TEXT("WALL %s asset=%s collision=%d camera=%d simple=%d trace=%d\n"), *It->GetActorLabel(), *Asset->GetName(), (int)Mesh->GetCollisionEnabled(), (int)Mesh->GetCollisionResponseToChannel(ECC_Camera), Body ? Body->AggGeom.GetElementCount() : -1, Body ? (int)Body->CollisionTraceFlag : -1);
                }
            }
        }
    }
    return FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("DreamVeilUIAudit.txt"))) ? 0 : 3;
}
