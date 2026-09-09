// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"

AMonsterAIController::AMonsterAIController()
{
	
}

void AMonsterAIController::StartBehaviorTree()
{
	if (!BehaviorTreeAsset) return;
	RunBehaviorTree(BehaviorTreeAsset);
	UE_LOG(LogTemp, Warning, TEXT("BT Loaded Successfully"));
}

void AMonsterAIController::BeginPlay()
{
	Super::BeginPlay();

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	
	StartBehaviorTree();
}

void AMonsterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (!InPawn) return;


}
