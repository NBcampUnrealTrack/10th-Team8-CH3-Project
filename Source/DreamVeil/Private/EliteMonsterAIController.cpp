// Fill out your copyright notice in the Description page of Project Settings.


#include "EliteMonsterAIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "BehaviorTree/BlackboardComponent.h"

AEliteMonsterAIController::AEliteMonsterAIController()
{
	// AI 감각 관련 컴포넌트들 추가하고 Set시켜주기
	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AI Perception"));
	SetPerceptionComponent(*AIPerception);
	
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight Config"));
	
	// 범위값 지정. LoseSightRadius는 인식한 대상을 놓치는 거리, PeripheralVisionAngleDegrees는 시야각/2임.
	SightConfig->SightRadius = 1500.0f;
	SightConfig->LoseSightRadius = 1800.0f;
	SightConfig->PeripheralVisionAngleDegrees = 60.0f;

	// 진영 인식
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

	// AIPerception에다가 아까 설정한 SightConfig을 레퍼런스변수로 등록.
	AIPerception->ConfigureSense(*SightConfig);
	// 그리고 등록한 SightConfig을 대표 감각으로 설정
	AIPerception->SetDominantSense(SightConfig->GetSenseImplementation());
}

void AEliteMonsterAIController::BeginPlay()
{
	Super::BeginPlay();

	// 이벤트 등록
	AIPerception->OnTargetPerceptionUpdated.AddDynamic(
		this,
		&AEliteMonsterAIController::OnTargetPerceptionUpdated);
}

void AEliteMonsterAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Blackboard) return;

	if (Stimulus.WasSuccessfullySensed())
	{
		Blackboard->SetValueAsObject(TEXT("SpottedActor"), Actor);
	}
	else
	{
		Blackboard->ClearValue(TEXT("SpottedActor"));
	}
}
