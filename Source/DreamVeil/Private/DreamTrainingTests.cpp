#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "DreamTrainingSystem.h"
#include "DreamLobbyWidgets.h"
#include "DreamVeilGameInstance.h"
#include "DreamAugmentCatalog.h"
#include "DispatchTableComponent.h"
#include "InventoryComponent.h"
#include "MainPlayerCharacter.h"
#include "MainPlayerController.h"
#include "MonsterSpawnVolume.h"
#include "CombatStatsComponent.h"
#include "MonsterProgressionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "BossMonster.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/UnrealType.h"
namespace DreamTrainingTests
{
struct FWorld
{
    UDreamVeilGameInstance* GI;UWorld* World;
    explicit FWorld(const TCHAR* Map=TEXT("Lobby"))
    {
        GI=NewObject<UDreamVeilGameInstance>(GEngine);GI->AddToRoot();
        GI->InitializeStandalone(FName(Map),CreatePackage(*(FString(TEXT("/Temp/DreamTraining/"))+FGuid::NewGuid().ToString()+TEXT("/")+Map)));
        World=GI->GetWorld();World->InitializeActorsForPlay(FURL());World->BeginPlay();World->GetWorldSettings()->NotifyBeginPlay();
    }
    AMainPlayerCharacter* Player(bool Native=false)
    {
        auto* P=World->SpawnActor<AMainPlayerCharacter>(FVector(0,0,10000),FRotator::ZeroRotator);
        if(Native)
        {
            auto* PC=World->SpawnActorDeferred<AMainPlayerController>(AMainPlayerController::StaticClass(),FTransform::Identity);
            auto* Local=NewObject<ULocalPlayer>(GEngine);GI->AddLocalPlayer(Local,FPlatformUserId::CreateFromInternalId(0));Local->PlayerController=PC;PC->Player=Local;
            PC->FinishSpawning(FTransform::Identity);PC->Possess(P);
        }
        else World->SpawnActor<APlayerController>()->Possess(P);return P;
    }
    ~FWorld(){GI->Shutdown();GEngine->DestroyWorldContext(World);World->DestroyWorld(false);GI->RemoveFromRoot();}
};
struct FSaveScope
{
    USaveGame* Before=UGameplayStatics::LoadGameFromSlot(TEXT("DreamVeilTraining"),0);
    FSaveScope(){if(Before)Before->AddToRoot();UGameplayStatics::SaveGameToSlot(NewObject<UDreamTrainingSaveGame>(),TEXT("DreamVeilTraining"),0);}
    ~FSaveScope(){if(Before){UGameplayStatics::SaveGameToSlot(Before,TEXT("DreamVeilTraining"),0);Before->RemoveFromRoot();}else UGameplayStatics::DeleteGameInSlot(TEXT("DreamVeilTraining"),0);}
};
}
#define TRAIN_TEST(C,N) IMPLEMENT_SIMPLE_AUTOMATION_TEST(C,"DreamVeil.Training." N,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
TRAIN_TEST(FDreamTrainingPermanent,"PermanentCostsPaymentAndIdempotentLoad")
bool FDreamTrainingPermanent::RunTest(const FString&)
{
    DreamTrainingTests::FSaveScope Save;DreamTrainingTests::FWorld W;auto* P=W.Player();auto* T=W.GI->GetSubsystem<UDreamTrainingSystem>();
    const float HP=P->CombatStats->GetMaxHealth();const int32 Cash=P->Inventory->GetDreamShards();
    TestFalse(TEXT("Insufficient funds do not grant upgrades"),T->Upgrade(P,EDreamTrainingStat::Health));TestEqual(TEXT("No debit on failure"),P->Inventory->GetDreamShards(),Cash);
    P->Inventory->AddDreamShards(10000);TestTrue(TEXT("Paid HP upgrade"),T->Upgrade(P,EDreamTrainingStat::Health));
    TestEqual(TEXT("Exactly 100 shards charged"),P->Inventory->GetDreamShards(),Cash+9900);
    TestEqual(TEXT("HP +10"),P->CombatStats->GetMaxHealth(),HP+10);
    T->ApplyToPlayer(P);T->ApplyToPlayer(P);TestEqual(TEXT("Applying again never compounds"),P->CombatStats->GetMaxHealth(),HP+10);
    TestTrue(TEXT("Same-stat price rises"),T->GetUpgradeCost(EDreamTrainingStat::Health)>100);
    TestTrue(TEXT("Other stats also follow total investment"),T->GetUpgradeCost(EDreamTrainingStat::Attack)>100);
    {DreamTrainingTests::FWorld Loaded;auto* Copy=Loaded.Player();TestEqual(TEXT("New GI restores saved permanent level"),Loaded.GI->GetSubsystem<UDreamTrainingSystem>()->GetLevel(EDreamTrainingStat::Health),1);TestEqual(TEXT("New pawn receives permanent HP"),Copy->CombatStats->GetMaxHealth(),HP+10);}
    for(int32 L=0;L<60;++L)TestTrue(TEXT("Costs monotonic and overflow safe"),UDreamTrainingSystem::CalculateUpgradeCost(L+1,L+1)>=UDreamTrainingSystem::CalculateUpgradeCost(L,L));
    return true;
}
TRAIN_TEST(FDreamRemovalRecipe,"AllRecipesLoseEvolutionWhenAnyIngredientRemoved")
bool FDreamRemovalRecipe::RunTest(const FString&)
{
    DreamTrainingTests::FWorld W;
    for(const auto& Recipe:DreamAugmentCatalog::All())if(Recipe.Evolution)
    {
        auto* P=W.Player();auto* D=P->DispatchTable.Get();auto H=Recipe.Ingredients;H.Add(Recipe.ID);D->RestoreAugments(H);
        TestTrue(*Recipe.Code,D->HasAcquiredAugment(Recipe.ID));
        const auto Removed=D->GetRemovalConsequences(Recipe.Ingredients[0]);TestTrue(TEXT("Removal preview includes learned evolution"),Removed.Contains(Recipe.ID));
        TestTrue(TEXT("Remove ingredient"),D->RemoveOwnedAugment(Recipe.Ingredients[0]));
        TestFalse(TEXT("Evolution is gone"),D->HasAcquiredAugment(Recipe.ID));TestEqual(TEXT("Removed skill frees its slot"),D->GetSkillCount(),0);
        const auto Saved=D->GetAugmentHistory();P->Destroy();auto* Copy=W.Player();Copy->DispatchTable->RestoreAugments(Saved);TestFalse(TEXT("Save/load cannot resurrect orphaned evolution"),Copy->DispatchTable->HasAcquiredAugment(Recipe.ID));Copy->Destroy();
    }
    auto* P=W.Player();P->DispatchTable->RestoreAugments({EAugmentID::SuppressionPulse});TestFalse(TEXT("Malformed old save skips missing recipe"),P->DispatchTable->HasAcquiredAugment(EAugmentID::SuppressionPulse));
    return true;
}
TRAIN_TEST(FDreamRemovalStats,"PaidRemovalRestoresStatsAndLastBreathCap")
bool FDreamRemovalStats::RunTest(const FString&)
{
    DreamTrainingTests::FSaveScope Save;DreamTrainingTests::FWorld W;auto* P=W.Player();auto* D=P->DispatchTable.Get();auto* S=P->CombatStats.Get();auto* T=W.GI->GetSubsystem<UDreamTrainingSystem>();
    const float HP=S->GetMaxHealth(),Attack=S->GetAttackPower();
    D->ApplyAugment(EAugmentID::AttackUp);D->ApplyAugment(EAugmentID::AttackUp);D->ApplyAugment(EAugmentID::HealthUp);
    TestFalse(TEXT("Removal requires payment"),T->RemoveAugment(P,EAugmentID::AttackUp));TestEqual(TEXT("No stacks lost on insufficient funds"),D->GetStackCount(EAugmentID::AttackUp),2);
    P->Inventory->AddDreamShards(10000);TestTrue(TEXT("Remove all attack stacks"),T->RemoveAugment(P,EAugmentID::AttackUp));TestEqual(TEXT("Only original attack remains"),S->GetAttackPower(),Attack);TestEqual(TEXT("Exact first removal fee"),P->Inventory->GetDreamShards(),9800);
    TestTrue(TEXT("Removal price rises"),T->GetRemovalCost()>200);TestTrue(TEXT("Reacquisition available"),D->CanAcquireAugment(EAugmentID::AttackUp));
    D->RemoveOwnedAugment(EAugmentID::HealthUp);TestEqual(TEXT("Health bonus removed"),S->GetMaxHealth(),HP);
    auto* Q=W.Player();auto* E=Q->DispatchTable.Get();auto* C=Q->CombatStats.Get();const float Defence=C->GetDefencePower();
    E->RestoreAugments({EAugmentID::Berserker,EAugmentID::LastFortress,EAugmentID::LastBreath});TestTrue(TEXT("Evolution cap applied"),C->GetCurrentHealthLimit()<=C->GetMaxHealth()*.201f);
    E->RemoveOwnedAugment(EAugmentID::Berserker);TestFalse(TEXT("Last Breath removed with recipe"),E->HasAcquiredAugment(EAugmentID::LastBreath));TestEqual(TEXT("Full healing limit restored"),C->GetCurrentHealthLimit(),C->GetMaxHealth());
    C->Heal(100000);TestEqual(TEXT("No residual Last Breath or fortress defence"),C->GetDefencePower(),Defence);
    return true;
}
TRAIN_TEST(FDreamJumpPause,"AirborneMomentumSurvivesPauseAndResume")
bool FDreamJumpPause::RunTest(const FString&)
{
    DreamTrainingTests::FWorld W;auto* P=W.Player(true);auto* PC=Cast<AMainPlayerController>(P->GetController());auto* M=P->GetCharacterMovement();
    M->SetMovementMode(MOVE_Falling);const FVector Before(700,120,350);M->Velocity=Before;
    PC->SetGameSuspended(true);TestTrue(TEXT("Pause preserves full jump velocity"),M->Velocity.Equals(Before));
    PC->SetGameSuspended(false);TestTrue(TEXT("Resume preserves full jump velocity"),M->Velocity.Equals(Before));
    TestFalse(TEXT("Input restored"),PC->IsMoveInputIgnored());
    M->SetMovementMode(MOVE_Walking);M->Velocity=Before;PC->SetGameSuspended(true);TestTrue(TEXT("Grounded stale movement still clears"),M->Velocity.IsNearlyZero());PC->SetGameSuspended(false);return true;
}
TRAIN_TEST(FDreamSpawnCooldown,"FailedSpawnHasBoundedRetryCooldown")
bool FDreamSpawnCooldown::RunTest(const FString&)
{
    DreamTrainingTests::FWorld W(TEXT("LV_1"));W.Player();auto* V=W.World->SpawnActor<AMonsterSpawnVolume>();
    TestEqual(TEXT("No nav geometry fails safely"),V->SpawnPackNow(),0);TestTrue(TEXT("Failure still cools down this location"),V->IsSpawnPointCoolingDown());
    const int32 Failures=V->SpawnLocationFailures;for(int32 I=0;I<20;++I)V->SpawnPackNow();TestEqual(TEXT("Repeated requests do not repeat searches or queue"),V->SpawnLocationFailures,Failures);
    return true;
}
TRAIN_TEST(FDreamReplayRewards,"CampaignReplayHalfFirstClearAndEndlessFull")
bool FDreamReplayRewards::RunTest(const FString&)
{
    for(int32 Level=1;Level<=4;++Level)
    {
        TestEqual(TEXT("First entry is full"),UDreamVeilGameInstance::CampaignRewardScale(Level,Level-1,false),1.f);
        TestEqual(TEXT("Already cleared campaign entry is half"),UDreamVeilGameInstance::CampaignRewardScale(Level,Level,false),.5f);
        TestEqual(TEXT("Endless always full"),UDreamVeilGameInstance::CampaignRewardScale(Level,4,true),1.f);
    }
    for(const TCHAR* Map:{TEXT("LV_1"),TEXT("LV_2"),TEXT("LV_3"),TEXT("LV_4"),TEXT("Endless")})
    {
        DreamTrainingTests::FWorld W(Map);W.GI->CheatSetClearedLevelCount(4);
        const float Scale=W.GI->GetCampaignRewardMultiplier();TestEqual(TEXT("Actual map reward classification"),Scale,FString(Map)==TEXT("Endless")?1.f:.5f);
        W.GI->CheatSetClearedLevelCount(0);TestEqual(TEXT("Entry snapshot remains fixed during run"),W.GI->GetCampaignRewardMultiplier(),Scale);
    }
    return true;
}
TRAIN_TEST(FDreamChoiceCategoryGuarantee,"FreeCategorySlotsReceiveNewSpeciesOnInitialAndRerollChoices")
bool FDreamChoiceCategoryGuarantee::RunTest(const FString&)
{
    DreamTrainingTests::FWorld W;auto* P=W.Player();auto* D=P->DispatchTable.Get();TArray<EAugmentID> Previous;
    for(int32 I=0;I<150;++I)
    {
        TArray<EAugmentID> Offer;D->DrawReplacementChoices(Previous,Offer);bool Passive=false,Active=false;TSet<EAugmentID> Unique;
        for(auto ID:Offer){Unique.Add(ID);if(!D->IsEvolution(ID)){Passive|=D->GetAugmentCategory(ID)==EAugmentCategory::Passive;Active|=D->GetAugmentCategory(ID)==EAugmentCategory::Active;}}
        TestTrue(TEXT("Both categories guaranteed while both have room"),Passive&&Active);TestEqual(TEXT("No duplicates"),Unique.Num(),Offer.Num());Previous=Offer;
    }
    TArray<EAugmentID> Owned;for(const auto& Def:DreamAugmentCatalog::All())if(!Def.Evolution&&Def.Category==EAugmentCategory::Passive&&Owned.Num()<6)Owned.Add(Def.ID);D->RestoreAugments(Owned);
    for(int32 I=0;I<60;++I)
    {
        TArray<EAugmentID> Offer;D->DrawReplacementChoices({},Offer);bool NewActive=false;
        for(auto ID:Offer)if(!D->IsEvolution(ID)){if(D->GetAugmentCategory(ID)==EAugmentCategory::Active)NewActive=true;else TestTrue(TEXT("Full passive category only offers owned stacks"),D->HasAcquiredAugment(ID));}
        TestTrue(TEXT("Open active slot is never starved by stat stacks"),NewActive);
    }
    return true;
}
TRAIN_TEST(FDreamInventoryActualLayout,"ActualBlueprintRootSupportsImageEquipmentOverlayAndStableIndices")
bool FDreamInventoryActualLayout::RunTest(const FString&)
{
    DreamTrainingTests::FWorld W;auto* P=W.Player(true);auto* PC=Cast<AMainPlayerController>(P->GetController());
    const auto Class=LoadClass<UUserWidget>(nullptr,TEXT("/Game/UI/WBP_Inventory.WBP_Inventory_C"));if(!TestNotNull(TEXT("Actual inventory blueprint"),Class))return false;
    for(auto Slot:{EWeaponPartSlot::Muzzle,EWeaponPartSlot::Magazine,EWeaponPartSlot::Sight}){FWeaponPart Part;Part.Slot=Slot;P->Inventory->AddPart(Part);}
    P->Inventory->EquipPart(0);auto* Host=CreateWidget<UUserWidget>(PC,Class);auto Slate=Host->TakeWidget();UDreamInventoryWidget::Install(Host);
    TArray<UUserWidget*> Found;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W.World,Found,UDreamInventoryWidget::StaticClass(),false);
    if(!TestEqual(TEXT("Responsive root receives one native panel"),Found.Num(),1))return false;
    auto* Panel=Cast<UDreamInventoryWidget>(Found[0]);TestEqual(TEXT("Equipped part excluded from owned rows"),Panel->GetVisibleOwnedCount(),2);
    Panel->HandleAction(10001);TestTrue(TEXT("Filtered row uses original index"),P->Inventory->GetPartsView()[1].bEquipped);TestEqual(TEXT("Newly equipped row disappears"),Panel->GetVisibleOwnedCount(),1);
    Panel->HandleAction(20000);TestEqual(TEXT("Unequipping restores owned row"),Panel->GetVisibleOwnedCount(),2);
    UDreamInventoryWidget::Install(Host);Found.Reset();UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W.World,Found,UDreamInventoryWidget::StaticClass(),false);TestEqual(TEXT("Reopening reuses the panel"),Found.Num(),1);return true;
}
TRAIN_TEST(FDreamReplayPayouts,"ActualKillRewardsHalfOnlyOnReplayedCampaign")
bool FDreamReplayPayouts::RunTest(const FString&)
{
    for(const TCHAR* Map:{TEXT("LV_1"),TEXT("Endless")})
    {
        DreamTrainingTests::FWorld First(Map);const float FullXP=UMonsterProgressionLibrary::GetExperienceReward(First.World,EMonsterGrade::Boss);
        DreamTrainingTests::FWorld Repeat(Map);Repeat.GI->CheatSetClearedLevelCount(4);auto* P=Repeat.Player();const int32 Before=P->Inventory->GetDreamShards();
        auto* Boss=Repeat.World->SpawnActor<ABossMonster>(FVector(10000,10000,10000),FRotator::ZeroRotator);const float Expected=FString(Map)==TEXT("Endless")?1.f:.5f;
        P->Inventory->ReceiveKillRewards(Boss);TestEqual(TEXT("Actual boss currency payout"),P->Inventory->GetDreamShards()-Before,int32(50*Expected));TestEqual(TEXT("Actual boss XP payout"),UMonsterProgressionLibrary::GetExperienceReward(Repeat.World,EMonsterGrade::Boss),FullXP*Expected);
        if(Expected==1.f)TestEqual(TEXT("Endless guaranteed boss item stays intact"),P->Inventory->GetPartsView().Num(),1);
    }
    return true;
}
#endif
