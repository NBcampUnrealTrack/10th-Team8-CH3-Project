// Fill out your copyright notice in the Description page of Project Settings.


#include "HordeMonster.h"
#include "DispatchTableComponent.h"
#include "Components/CapsuleComponent.h"
#include "MonsterAIController.h"


AHordeMonster::AHordeMonster()
{
	MonsterCapsuleCollisionComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule Collision"));
	MonsterCollisionComponent = MonsterCapsuleCollisionComponent;
	MonsterCollisionComponent->SetupAttachment(RootComponent);

	AIControllerClass = AMonsterAIController::StaticClass();
	
	// SkeletalMeshComponent만의 고유한 기능을 쓸 수도 있으니 이렇게 두 변수로 나눕니다. 둘 다 가리키는 컴포넌트는 동일
	MonsterSkeletalMeshComponent = GetMesh();
	MonsterMeshComponent->SetupAttachment(RootComponent);
	
	this->MaxHealth = FMath::RandRange(100.0f, 150.0f);
	MonsterDispatchTable->OnMaxHealthChanged.AddDynamic(
		this,
		&AHordeMonster::MaxHealthChanged
	);
	MonsterDispatchTable->SetMaxHealth(MaxHealth);

	MonsterWalkSpeed = 500.0f;
}

void AHordeMonster::BeginPlay()
{
	Super::BeginPlay();
	
}

void AHordeMonster::MaxHealthChanged(float OldValue, float NewValue)
{
	//쓸라했는데 생각해보니 쓸일없을거같음
}
void AHordeMonster::MonsterInit()
{
	//몬스터 생성 시 초기화, 오브젝트 풀링에 사용할 수도 있어서 따로 빼놓음
	MonsterDispatchTable->SetCurrentHealth(MaxHealth);
}