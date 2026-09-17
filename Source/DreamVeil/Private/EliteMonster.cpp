#include "EliteMonster.h"
#include "CombatStatsComponent.h"
#include "Components/CapsuleComponent.h"
#include "EliteMonsterAIController.h"

AEliteMonster::AEliteMonster()
{
	MonsterCapsuleCollisionComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule Collision"));
	MonsterCollisionComponent = MonsterCapsuleCollisionComponent;
	MonsterCollisionComponent->SetupAttachment(RootComponent);

	AIControllerClass = AEliteMonsterAIController::StaticClass();

	// SkeletalMeshComponent만의 고유한 기능을 쓸 수도 있으니 이렇게 두 변수로 나눕니다. 둘 다 가리키는 컴포넌트는 동일
	MonsterSkeletalMeshComponent = GetMesh();
	MonsterMeshComponent->SetupAttachment(RootComponent);

	this->MaxHealth = FMath::RandRange(100.0f, 150.0f);
	MonsterCombatStats->SetMaxHealth(MaxHealth);
	MonsterWalkSpeed = 500.0f;
}

void AEliteMonster::BeginPlay()
{
}
