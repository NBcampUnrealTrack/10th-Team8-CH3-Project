// Fill out your copyright notice in the Description page of Project Settings.


#include "BTService_CombatState.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "MonsterBase.h"
#include "BehaviorTree/BlackboardComponent.h"

//공격 가능 여부를 적어 둘 블랙보드 칸 이름
//TEXT()를 그대로 넘기지 않고 한 번만 만들어 두는 이유 넘길 때마다 FName을 새로 만들며 문자열을 해시함
//이 서비스는 살아 있는 몬스터마다 매 틱 돌아서 몬스터가 늘어나면 그 비용도 몬스터 수만큼 그대로 늘어남
static const FName IS_ATTACK_ABLE_KEY(TEXT("IsAttackAble"));

//공격 반경을 못 읽었을 때 쓸 기본 거리
static const float FALLBACK_COMBAT_DISTANCE = 200.0f;

UBTService_CombatState::UBTService_CombatState()
{
	//BT에서 보여질거
	NodeName = TEXT("Update CombatState");
}

void UBTService_CombatState::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	//적을 곳부터 확인함 블랙보드가 없으면 거리를 재 봐야 쓸 데가 없음
	UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();

	if (!Blackboard)
	{
		return;
	}

	//플레이어 위치를 가져오기 위한 플레이어 캐스팅
	//플레이어가 죽어 폰이 사라졌거나 레벨을 넘기는 중이면 nullptr이 옴
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);

	//해당 BT를 가지고 있는 ai 컨트롤러 가져오기
	AAIController* AIController = OwnerComp.GetAIOwner();

	//AI컨트롤러의 소유자, 곧 몬스터 가져오기
	//몬스터가 죽으면 DetachFromControllerPendingDestroy로 폰이 떨어져 나가서 여기가 nullptr이 됨
	//예전에는 바로 GetPawn()을 불러서 몬스터가 죽는 순간 서비스가 한 번 더 돌면 크래시가 날 수 있었음
	APawn* AIPawn = AIController ? AIController->GetPawn() : nullptr;

	if (!PlayerPawn || !AIPawn)
	{
		//판단할 수 없을 때는 공격 불가로 적어 둠 그냥 두면 직전 true가 남아서 없는 상대를 때림
		Blackboard->SetValueAsBool(IS_ATTACK_ABLE_KEY, false);
		return;
	}

	//플레이어와 몬스터 사이의 거리 계산
	const double Distance = FVector::Distance(AIPawn->GetActorLocation(), PlayerPawn->GetActorLocation());

	// 몬스터의 공격 반경, 즉 공격할 수 있는 거리(근접, 원거리)
	//Cast 결과를 바로 쓰지 않는 이유 몬스터가 아닌 폰이 이 행동트리를 쓰면 nullptr이 되어 크래시가 남
	const AMonsterBase* Monster = Cast<AMonsterBase>(AIPawn);
	const float MonsterAttackRange = Monster ? Monster->GetMonsterAttackRange() : 0.0f;

	// 만약 기본값 가져오기 실패하면 200으로 고정
	const float CombatDistance = MonsterAttackRange > 0.0f ? MonsterAttackRange : FALLBACK_COMBAT_DISTANCE;

	// 해당 근접공격 때리기 가능하면 IsAttackAble 변수 변경
	//매 틱 같은 값을 적어도 괜찮음 SetValueAsBool은 값이 바뀔 때만 관찰자에게 알림
	Blackboard->SetValueAsBool(IS_ATTACK_ABLE_KEY, Distance <= CombatDistance);
}