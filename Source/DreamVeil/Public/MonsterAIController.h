// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "MonsterAIController.generated.h"

class UAIPerceptionComonent;
class UAISenseConfig_Sight;
class UBehaviorTree;

UCLASS()
class DREAMVEIL_API AMonsterAIController : public AAIController
{
	GENERATED_BODY()
	
public:
	AMonsterAIController();
	void StartBehaviorTree();
	virtual void BeginPlay() override;

	// 레그돌 상태를 BT에 전달하는 함수.
	void SetRagdollState(bool bEnabled);

	//스킬이 직접 이동하는 동안 BT의 추격 요청을 잠시 멈추고 끝나면 다시 진행한다.
	void SetSkillMovementLocked(bool bLocked);
    void CheckChaseProgress(APawn* Target);
    virtual void OnUnPossess() override;
    UPROPERTY(VisibleInstanceOnly,Category="AI|Recovery") int32 PathRecoveryCount=0;

protected:
	virtual void OnPossess(APawn* InPawn) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|BehaviorTree")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

private:
	//다른 이유로 멈춘 BT를 실수로 재개하지 않도록 이 함수가 멈췄는지 기록한다.
	bool bBrainPausedForSkill = false;
    FVector ProgressLocation=FVector::ZeroVector;
    float NextProgressCheck=0,StalledSince=0,NextRecovery=0;
    uint32 ProgressGeneration=0;
    bool bRecoveryPaused=false;
    FTimerHandle RecoveryTimer;
    void FinishRecovery();
	FTimerHandle RandomPatrolTime;

	UPROPERTY(EditAnywhere, Category="AI")
	float MoveRadius;


};
