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

protected:
	virtual void OnPossess(APawn* InPawn) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|BehaviorTree")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

private:
	//다른 이유로 멈춘 BT를 실수로 재개하지 않도록 이 함수가 멈췄는지 기록한다.
	bool bBrainPausedForSkill = false;
	FTimerHandle RandomPatrolTime;

	UPROPERTY(EditAnywhere, Category="AI")
	float MoveRadius;


};
