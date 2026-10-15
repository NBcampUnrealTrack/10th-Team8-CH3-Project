// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterAIController.h"
#include "MonsterBase.h"
#include "MonsterSkill.h"
#include "CombatStatsComponent.h"
#include "DreamPlayPolish.h"
#include "DreamObjectPoolSubsystem.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
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
    GetWorldTimerManager().ClearTimer(RecoveryTimer);bRecoveryPaused=false;bBrainPausedForSkill=false;StalledSince=NextProgressCheck=NextRecovery=0;ProgressGeneration=0;
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
        if(bRecoveryPaused){GetWorldTimerManager().ClearTimer(RecoveryTimer);bRecoveryPaused=false;if(BrainComponent&&!bBrainPausedForSkill)BrainComponent->ResumeLogic(TEXT("Recovery interrupted by ragdoll"));}
		// 이동멈추기, AIController 기본에 내장기능임
		StopMovement();
	}

}

void AMonsterAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
    NextProgressCheck=StalledSince=NextRecovery=0;ProgressGeneration=0;PathRecoveryCount=0;
	if (!InPawn) return;
}

void AMonsterAIController::SetSkillMovementLocked(bool bLocked)
{
	UBrainComponent* Brain = GetBrainComponent();
	if (bLocked)
	{
        if(bRecoveryPaused){GetWorldTimerManager().ClearTimer(RecoveryTimer);bRecoveryPaused=false;bBrainPausedForSkill=true;}
		if (Brain && Brain->IsRunning() && !Brain->IsPaused())
		{
			bBrainPausedForSkill = true;
			Brain->PauseLogic(TEXT("Monster skill"));
		}
		//BT를 먼저 멈춰야 이동 취소 후 곧바로 새 추격을 요청하지 않는다.
		StopMovement();
	}
	else if (bBrainPausedForSkill)
	{
		bBrainPausedForSkill = false;
		if (Brain) Brain->ResumeLogic(TEXT("Monster skill finished"));
	}
}

void AMonsterAIController::OnUnPossess()
{GetWorldTimerManager().ClearTimer(RecoveryTimer);bRecoveryPaused=false;bBrainPausedForSkill=false;Super::OnUnPossess();}
void AMonsterAIController::CheckChaseProgress(APawn* Target)
{
    auto* M=Cast<AMonsterBase>(GetPawn());if(!M||!Target||!GetWorld())return;const float Now=GetWorld()->GetTimeSeconds();if(Now<NextProgressCheck)return;NextProgressCheck=Now+.6f;
    const uint32 Generation=UDreamObjectPoolSubsystem::Generation(M);
    const bool Busy=M->IsHidden()||!M->MonsterCombatStats||M->MonsterCombatStats->IsDead()||M->IsRagdoll()||M->IsAttacking()||M->IsAugmentStaggered()||(M->MonsterSkill&&M->MonsterSkill->IsUsingSkill())||M->GetCharacterMovement()->IsFalling()||bRecoveryPaused;
    if(Busy||Generation!=ProgressGeneration||FVector::DistSquared2D(M->GetActorLocation(),Target->GetActorLocation())<=FMath::Square(M->GetMonsterAttackRange()+80)||FVector::DistSquared2D(M->GetActorLocation(),ProgressLocation)>FMath::Square(45.f))
    {ProgressLocation=M->GetActorLocation();ProgressGeneration=Generation;StalledSince=Now;return;}
    if(Now-StalledSince<2.4f||Now<NextRecovery||!BrainComponent||!BrainComponent->IsRunning()||BrainComponent->IsPaused())return;
    if(auto* Flow=GetWorld()->GetSubsystem<UDreamEncounterDirector>())if(!Flow->AllowPathRecovery())return;
    NextRecovery=Now+4.f;auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!Nav)return;
    // No teleport: select a short reachable detour and let CharacterMovement walk it.
    FNavLocation Point;if(!Nav->GetRandomReachablePointInRadius(M->GetNavAgentLocation(),300,Point))return;
    if(FVector::DistSquared2D(Point.Location,M->GetActorLocation())<FMath::Square(75.f))return;
    BrainComponent->PauseLogic(TEXT("Short blocked-path recovery"));bRecoveryPaused=true;StopMovement();
    MoveToLocation(Point.Location,40,true,true,true,false,nullptr,false);++PathRecoveryCount;
    GetWorldTimerManager().SetTimer(RecoveryTimer,this,&AMonsterAIController::FinishRecovery,1.1f,false);
}
void AMonsterAIController::FinishRecovery()
{
    if(!bRecoveryPaused)return;bRecoveryPaused=false;auto* M=Cast<AMonsterBase>(GetPawn());StopMovement();
    if(M&&!M->IsHidden()&&M->MonsterCombatStats&&!M->MonsterCombatStats->IsDead()&&!M->IsRagdoll()&&!bBrainPausedForSkill)
        if(BrainComponent)BrainComponent->ResumeLogic(TEXT("Retry moving player target"));
    StalledSince=GetWorld()->GetTimeSeconds();ProgressLocation=M?M->GetActorLocation():FVector::ZeroVector;
}
