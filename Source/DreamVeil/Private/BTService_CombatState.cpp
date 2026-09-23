// Fill out your copyright notice in the Description page of Project Settings.


#include "BTService_CombatState.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "MonsterBase.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTService_CombatState::UBTService_CombatState()
{
	//BT에서 보여질거
	NodeName = TEXT("Update CombatState");
}

void UBTService_CombatState::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
	//플레이어 위치를 가져오기 위한 플레이어 캐스팅
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	//해당 BT를 가지고 있는 ai 컨트롤러 가져오기
	AAIController* AIController = OwnerComp.GetAIOwner();
	//AI컨트롤러의 소유자, 곧 몬스터 가져오기
	APawn* AIPawn = AIController->GetPawn();

	//플레이어와 몬스터 사이의 거리 계산
	double Distance = FVector::Distance(AIPawn->GetActorLocation(), PlayerPawn->GetActorLocation());

	// 몬스터의 공격 반경, 즉 공격할 수 있는 거리(근접, 원거리)
	float CombatDistance = Cast<AMonsterBase>(AIPawn)->GetMonsterAttackRange();
	// 만약 기본값 가져오기 실패하면 200으로 고정
	if (!CombatDistance) CombatDistance = 200.0f;

	// 해당 근접공격 때리기 가능하면 IsAttackAble 변수 변경
	if (Distance <= CombatDistance)
	{
		OwnerComp.GetBlackboardComponent()->SetValueAsBool(TEXT("IsAttackAble"), true);
	}
	else
	{
		OwnerComp.GetBlackboardComponent()->SetValueAsBool(TEXT("IsAttackAble"), false);
	}
}