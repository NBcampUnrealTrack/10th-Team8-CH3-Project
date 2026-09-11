// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterBase.h"
#include "MonsterAIController.h"
#include "Components/SphereComponent.h"
#include "HealthComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

// Sets default values
AMonsterBase::AMonsterBase()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::PlacedInWorld;
	MonsterHealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("MonsterHealthComponent"));
	MonsterCollisionComponent = nullptr;
	MonsterMeshComponent = GetMesh();

	MonsterWalkSpeed = 500.0f;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;

	GetCharacterMovement()->RotationRate = FRotator(0.0f, 360.0f, 0.0f);
	//MonsterCollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision")); 자식에서 이렇게 생성
}

// Called when the game starts or when spawned
void AMonsterBase::BeginPlay()
{
	Super::BeginPlay();
	
	GetCharacterMovement()->MaxWalkSpeed = MonsterWalkSpeed;
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

