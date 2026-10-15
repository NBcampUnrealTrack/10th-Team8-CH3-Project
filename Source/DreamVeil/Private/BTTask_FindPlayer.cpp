// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_FindPlayer.h"
#include "DreamAugmentRuntime.h"
#include "MonsterBase.h"
#include "DreamCrowdTacticsSubsystem.h"
#include "AIController.h"
#include <Kismet/GameplayStatics.h>
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_FindPlayer::UBTTask_FindPlayer()
{
	//BT에서 보여질 이름
	NodeName = TEXT("Find Player Location Task");
}
EBTNodeResult::Type UBTTask_FindPlayer::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	//BT에서 블랙보드 컴포넌트 가져오기
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();

	if (!BlackboardComp) return EBTNodeResult::Failed;

	// 플레이어 가져오기
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);

    if(auto* AI=OwnerComp.GetAIOwner())PlayerPawn=ADreamAugmentField::FindDecoyFor(Cast<AMonsterBase>(AI->GetPawn()),PlayerPawn);
    if (!PlayerPawn) return EBTNodeResult::Failed;
    BlackboardComp->SetValueAsObject(TEXT("PlayerActor"),PlayerPawn);


	FVector Approach=PlayerPawn->GetActorLocation();
    // MoveTo tracks PlayerActor continuously, including decoys.
    BlackboardComp->SetValueAsVector(TEXT("PlayerVec"),Approach);
	return EBTNodeResult::Succeeded;
}
