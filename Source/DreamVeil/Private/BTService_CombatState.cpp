// Fill out your copyright notice in the Description page of Project Settings.


#include "BTService_CombatState.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "MonsterBase.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTService_CombatState::UBTService_CombatState()
{
	NodeName = TEXT("Update CombatState");
}

void UBTService_CombatState::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	AAIController* AIController = OwnerComp.GetAIOwner();
	APawn* AIPawn = AIController->GetPawn();

	double Distance = FVector::Distance(AIPawn->GetActorLocation(), PlayerPawn->GetActorLocation());

	float CombatDistance = Cast<AMonsterBase>(AIPawn)->GetMonsterAttackRange();
	if (!CombatDistance) CombatDistance = 100.0f;
	if (Distance <= CombatDistance)
	{
		OwnerComp.GetBlackboardComponent()->SetValueAsBool(TEXT("IsAttackAble"), true);
	}
	else
	{
		OwnerComp.GetBlackboardComponent()->SetValueAsBool(TEXT("IsAttackAble"), false);
	}
}