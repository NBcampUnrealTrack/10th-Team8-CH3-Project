// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterBase.h"
#include "MonsterAIController.h"
#include "Components/SphereComponent.h"
#include "HealthComponent.h"

// Sets default values
AMonsterBase::AMonsterBase()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AMonsterAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorld;
	MonsterHealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("MonsterHealthComponent"));
	MonsterCollisionComponent = nullptr;
	MonsterMeshComponent = nullptr;
	//MonsterCollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision")); 자식에서 이렇게 생성
}

// Called when the game starts or when spawned
void AMonsterBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMonsterBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AMonsterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

