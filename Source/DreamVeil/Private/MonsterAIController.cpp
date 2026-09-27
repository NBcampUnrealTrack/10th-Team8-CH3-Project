// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "BehaviorTree//BehaviorTree.h"

AMonsterAIController::AMonsterAIController()
{
	static ConstructorHelpers::FObjectFinder<UBehaviorTree> BTObject(TEXT("/Game/Managers/AI/BT_Monster.BT_Monster"));
	if (BTObject.Succeeded())
	{
		BehaviorTreeAsset = BTObject.Object;
	}
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

	UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
	if (BlackboardComp)
	{
		BlackboardComp->SetValueAsObject(TEXT("PlayerActor"), PlayerPawn);
	}
}

void AMonsterAIController::SetRagdollState(bool bEnabled)
{
	//AAIController에 Blackboard라는 멤버가 이미 있어서 같은 이름을 쓰면 가려짐(C4458)
	//이 프로젝트는 경고를 오류로 다루므로 이름을 달리 둠
	if (UBlackboardComponent* BlackboardComponent = GetBlackboardComponent())
	{
		BlackboardComponent->SetValueAsBool(TEXT("IsRagdoll"), bEnabled);
	}
	if (bEnabled)
	{
		// 이동멈추기, AIController 기본에 내장기능임
		StopMovement();
	}

}

void AMonsterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (!InPawn) return;
}
