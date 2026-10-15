#include "BTTask_MonsterAttack.h"

#include "AIController.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"
#include "CombatStatsComponent.h"
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

    // Ranged enemies make space before shooting when the player crowds them.
    // One bounded navigation request and timer per retreat; no extra per-frame AI tick.
    const float AttackRange = Monster->GetMonsterAttackRange();
    const float Distance = FVector::Dist2D(Monster->GetActorLocation(), Target->GetActorLocation());
    if (Monster->GetAttackType() == EMonsterAttackType::Ranged && !Monster->IsRagdoll()
        && Monster->MonsterCombatStats && !Monster->MonsterCombatStats->IsDead()
        && AttackRange > 400 && Distance < FMath::Clamp(AttackRange * .35f, 180.0f, 450.0f))
    {
        if (auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(Monster->GetWorld()))
        {
            const FVector Away = (Monster->GetActorLocation()-Target->GetActorLocation()).GetSafeNormal2D();
            FNavLocation Point;
            const FVector Destination = Monster->GetNavAgentLocation() + Away * 450;
            if (!Away.IsNearlyZero() && Nav->ProjectPointToNavigation(Destination,Point,FVector(120,120,250))
                && FVector::Dist2D(Point.Location,Target->GetActorLocation()) > Distance+150
                && AIController->MoveToLocation(Point.Location,60,true,true,false,false,nullptr,false)
                    == EPathFollowingRequestResult::RequestSuccessful)
            {
                AttackingMonster=Monster;RunningBT=&OwnerComp;bRetreating=true;
                Monster->GetWorldTimerManager().SetTimer(RetreatTimer,this,&UBTTask_MonsterAttack::HandleRetreatFinished,1.0f,false);
                return EBTNodeResult::InProgress;
            }
        }
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

void UBTTask_MonsterAttack::HandleRetreatFinished()
{
    UBehaviorTreeComponent* BT=RunningBT.Get();
    ClearBinding();
    if(BT)FinishLatentTask(*BT,EBTNodeResult::Succeeded);
}

void UBTTask_MonsterAttack::ClearBinding()
{
    if (AMonsterBase* Monster = AttackingMonster.Get())
    {
        Monster->GetWorldTimerManager().ClearTimer(RetreatTimer);
        if(bRetreating)if(auto* AI=Cast<AAIController>(Monster->GetController()))AI->StopMovement();
        if (AttackFinishedHandle.IsValid())
        {
            Monster->OnAttackFinished.Remove(AttackFinishedHandle);
        }
    }

    bRetreating=false;
    AttackFinishedHandle.Reset();
    AttackingMonster.Reset();
    RunningBT.Reset();
}