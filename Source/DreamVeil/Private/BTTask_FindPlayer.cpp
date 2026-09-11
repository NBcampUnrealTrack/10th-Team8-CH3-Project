// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_FindPlayer.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"

UBTTask_FindPlayer::UBTTask_FindPlayer()
{
	NodeName = TEXT("Find Player Location");
}

EBTNodeResult::Type UBTTask_FindPlayer::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();

	if (!BlackboardComp) return EBTNodeResult::Failed;

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);

	if (!PlayerPawn) return EBTNodeResult::Failed;

	BlackboardComp->SetValueAsVector(TEXT("PlayerVec"), PlayerPawn->GetActorLocation());
	return EBTNodeResult::Succeeded;
}
