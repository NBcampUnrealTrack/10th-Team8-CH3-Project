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

int32 UDreamVeilUIUpdateCommandlet::Main(const FString& Params)
{
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
