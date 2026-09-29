#include "BTTask_MonsterAttack.h"

#include "AIController.h"
#include "MonsterBase.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_MonsterAttack::UBTTask_MonsterAttack()
{
    NodeName = TEXT("Monster Attack");

    // 이 노드가 여러 AI가 공유하는 노드가 아니라 각자 소유하는 인스턴스화 한 노드라는 뜻임
    bCreateNodeInstance = true;

    BlackboardKey.AddObjectFilter(
        this,
        GET_MEMBER_NAME_CHECKED(
            UBTTask_MonsterAttack,
            BlackboardKey),
        AActor::StaticClass());
}

EBTNodeResult::Type UBTTask_MonsterAttack::ExecuteTask(
    UBehaviorTreeComponent& OwnerComp,
    uint8* NodeMemory)
{
    ClearBinding();

    AAIController* AIController = OwnerComp.GetAIOwner();
    UBlackboardComponent* Blackboard =
        OwnerComp.GetBlackboardComponent();

    if (!AIController || !Blackboard)
    {
        return EBTNodeResult::Failed;
    }

    AMonsterBase* Monster =
        Cast<AMonsterBase>(AIController->GetPawn());

    AActor* Target = Cast<AActor>(
        Blackboard->GetValueAsObject(
            BlackboardKey.SelectedKeyName));

    if (!IsValid(Monster) || !IsValid(Target))
    {
        return EBTNodeResult::Failed;
    }

    AIController->StopMovement();

    AttackingMonster = Monster;
    RunningBT = &OwnerComp;

    AttackFinishedHandle =
        Monster->OnAttackFinished.AddUObject(
            this,
            &UBTTask_MonsterAttack::HandleAttackFinished);

    if (!Monster->StartAttack(Target))
    {
        ClearBinding();
        return EBTNodeResult::Failed;
    }

    return EBTNodeResult::InProgress;
}

void UBTTask_MonsterAttack::HandleAttackFinished(bool bSucceeded)
{
    UBehaviorTreeComponent* BT = RunningBT.Get();

    ClearBinding();

    if (BT)
    {
        FinishLatentTask(
            *BT,
            bSucceeded
            ? EBTNodeResult::Succeeded
            : EBTNodeResult::Failed);
    }
}

EBTNodeResult::Type UBTTask_MonsterAttack::AbortTask(
    UBehaviorTreeComponent& OwnerComp,
    uint8* NodeMemory)
{
    AMonsterBase* Monster = AttackingMonster.Get();

    // CancelAttack의 종료 알림을 받지 않도록 먼저 해제해주ㅁ
    ClearBinding();

    if (IsValid(Monster))
    {
        Monster->CancelAttack();
    }

    return EBTNodeResult::Aborted;
}

void UBTTask_MonsterAttack::ClearBinding()
{
    if (AMonsterBase* Monster = AttackingMonster.Get())
    {
        if (AttackFinishedHandle.IsValid())
        {
            Monster->OnAttackFinished.Remove(AttackFinishedHandle);
        }
    }

    AttackFinishedHandle.Reset();
    AttackingMonster.Reset();
    RunningBT.Reset();
}