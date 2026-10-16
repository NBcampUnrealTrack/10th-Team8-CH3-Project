#include "DreamTrainingSystem.h"
#include "DreamLobbyWidgets.h"
#include "MainPlayerCharacter.h"
#include "MainPlayerController.h"
#include "DreamVeilGameInstance.h"
#include "InventoryComponent.h"
#include "DispatchTableComponent.h"
#include "DreamAugmentCatalog.h"
#include "CombatStatsComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
namespace { const TCHAR* TrainingSlot=TEXT("DreamVeilTraining"); }
void UDreamTrainingSystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Progress=Cast<UDreamTrainingSaveGame>(UGameplayStatics::LoadGameFromSlot(TrainingSlot,0));
    if(!Progress)Progress=NewObject<UDreamTrainingSaveGame>(this);
    Progress->Levels.SetNum(4);
    for(auto& Level:Progress->Levels)Level=FMath::Clamp(Level,0,1000);
    Progress->RemovalCount=FMath::Clamp(Progress->RemovalCount,0,1000);
}
int32 UDreamTrainingSystem::GetLevel(EDreamTrainingStat Stat)const
{const int32 Index=static_cast<int32>(Stat);return Progress&&Progress->Levels.IsValidIndex(Index)?Progress->Levels[Index]:0;}
int32 UDreamTrainingSystem::CalculateUpgradeCost(int32 Level,int32 Total)
{
    const double Cost=100.*FMath::Pow(1.5,double(FMath::Clamp(Level,0,60)))*FMath::Pow(1.08,double(FMath::Clamp(Total,0,250)));
    return int32(FMath::Min(2000000000.,FMath::CeilToDouble(Cost)));
}
int32 UDreamTrainingSystem::GetUpgradeCost(EDreamTrainingStat Stat)const
{int32 Total=0;if(Progress)for(int32 Level:Progress->Levels)Total+=Level;return CalculateUpgradeCost(GetLevel(Stat),Total);}
int32 UDreamTrainingSystem::GetRemovalCost()const
{return int32(FMath::Min(2000000000.,FMath::CeilToDouble(200.*FMath::Pow(1.25,double(FMath::Min(Progress?Progress->RemovalCount:0,80))))));}
bool UDreamTrainingSystem::CanUse(AMainPlayerCharacter* Player)const
{
    const auto* GI=Player?Player->GetGameInstance<UDreamVeilGameInstance>():nullptr;
    return Progress&&GI&&GI==GetGameInstance()&&GI->IsInLobby()&&Player->Inventory&&Player->CombatStats&&!Player->CombatStats->IsDead()&&!Player->HasAugmentChoices();
}
bool UDreamTrainingSystem::SaveProgress()const{return Progress&&UGameplayStatics::SaveGameToSlot(Progress,TrainingSlot,0);}
void UDreamTrainingSystem::ApplyToPlayer(AMainPlayerCharacter* Player)
{
    if(!Player||!Player->CombatStats||!Progress)return;
    for(auto It=Applied.CreateIterator();It;++It)if(!It.Key().IsValid())It.RemoveCurrent();
    auto& Before=Applied.FindOrAdd(Player,FVector4(0,0,0,0));
    const FVector4 Bonus(GetLevel(EDreamTrainingStat::Health)*10.f,GetLevel(EDreamTrainingStat::Defence)*1.f,GetLevel(EDreamTrainingStat::Attack)*1.f,GetLevel(EDreamTrainingStat::Stamina)*5.f);
    auto* Stats=Player->CombatStats.Get();
    Stats->SetMaxHealth(Stats->GetMaxHealth()+float(Bonus.X-Before.X));
    Stats->AddDefencePower(float(Bonus.Y-Before.Y));Stats->AddAttackPower(float(Bonus.Z-Before.Z));
    Player->IncreaseMaxStamina(float(Bonus.W-Before.W));Before=Bonus;
}
bool UDreamTrainingSystem::Upgrade(AMainPlayerCharacter* Player,EDreamTrainingStat Stat)
{
    if(!CanUse(Player)||static_cast<uint8>(Stat)>3||GetLevel(Stat)>=1000)return false;
    const int32 Cost=GetUpgradeCost(Stat);auto* Inventory=Player->Inventory.Get();
    if(!Inventory->SpendDreamShards(Cost)){Message=FText::FromString(TEXT("꿈의 조각이 부족합니다."));return false;}
    ++Progress->Levels[static_cast<int32>(Stat)];
    auto* GI=Player->GetGameInstance<UDreamVeilGameInstance>();GI->SaveCurrentPlayerProgress();
    if(!SaveProgress()||!GI->SaveGameToSlot())
    {
        --Progress->Levels[static_cast<int32>(Stat)];Inventory->AddDreamShards(Cost);SaveProgress();GI->SaveCurrentPlayerProgress();GI->SaveGameToSlot();
        Message=FText::FromString(TEXT("저장하지 못해 강화와 결제를 취소했습니다."));return false;
    }
    ApplyToPlayer(Player);if(Stat==EDreamTrainingStat::Health)Player->CombatStats->Heal(10);
    Message=FText::FromString(TEXT("단련 완료 · 다음 플레이에도 유지됩니다."));return true;
}
bool UDreamTrainingSystem::RemoveAugment(AMainPlayerCharacter* Player,EAugmentID ID)
{
    if(!CanUse(Player)||!Player->DispatchTable||!Player->DispatchTable->CanRemoveAugment(ID))return false;
    const int32 Cost=GetRemovalCost();auto* Inventory=Player->Inventory.Get();
    if(!Inventory->SpendDreamShards(Cost)){Message=FText::FromString(TEXT("꿈의 조각이 부족합니다."));return false;}
    auto* GI=Player->GetGameInstance<UDreamVeilGameInstance>();GI->SaveCurrentPlayerProgress();
    // Persist the planned history before touching effects. Failed writes never consume an augment.
    const auto Removed=Player->DispatchTable->GetRemovalConsequences(ID);
    for(auto RemovedID:Removed)GI->SavedPlayerAugmentHistory.Remove(RemovedID);
    ++Progress->RemovalCount;
    if(!SaveProgress()||!GI->SaveGameToSlot())
    {
        --Progress->RemovalCount;Inventory->AddDreamShards(Cost);GI->SaveCurrentPlayerProgress();SaveProgress();GI->SaveGameToSlot();
        Message=FText::FromString(TEXT("저장하지 못해 증강 제거와 결제를 취소했습니다."));return false;
    }
    if(!Player->DispatchTable->RemoveOwnedAugment(ID))
    {
        --Progress->RemovalCount;Inventory->AddDreamShards(Cost);GI->SaveCurrentPlayerProgress();SaveProgress();GI->SaveGameToSlot();return false;
    }
    Message=FText::FromString(Removed.Num()>1?TEXT("증강과 조건이 깨진 진화를 함께 해제했습니다."):TEXT("증강의 모든 중첩을 제거했습니다. 다시 획득할 수 있습니다."));
    return true;
}
ADreamTrainingCabinet::ADreamTrainingCabinet()
{
    InteractionLabel=FText::FromString(TEXT("F 꿈의 단련소 입장"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Game/Maps/Asset/SM_Lobby_Cabinet"));
    GetObjectMesh()->SetStaticMesh(Mesh.Object);
}
void ADreamTrainingCabinet::Interact_Implementation(AActor* Interactor)
{
    auto* Pawn=Cast<AMainPlayerCharacter>(Interactor);auto* GI=Pawn?Pawn->GetGameInstance<UDreamVeilGameInstance>():nullptr;
    if(GI&&GI->IsInLobby())if(auto* PC=Cast<AMainPlayerController>(Pawn->GetController()))PC->OpenMenuWidgetPaused(UDreamTrainingWidget::StaticClass());
}
