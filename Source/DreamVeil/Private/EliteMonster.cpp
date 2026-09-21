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

	EliteType = EEliteMonsterType::Elite1; //일단 기본은 Elite1

	// 엘리트 체력 기본값. 바꿀거면 하세용
	MinHealthRadius = 300.0f;
	MaxHealthRadius = 500.0f;

	// SkeletalMeshComponent만의 고유한 기능을 쓸 수도 있으니 이렇게 두 변수로 나눕니다. 둘 다 가리키는 컴포넌트는 동일
	MonsterSkeletalMeshComponent = GetMesh();
	MonsterMeshComponent->SetupAttachment(RootComponent);

	MonsterInit(); 
}

void AEliteMonster::BeginPlay()
{
	Super::BeginPlay(); // 이 안에 MonsterInit 들어있음
}

void AEliteMonster::MonsterInit()
{
	Super::MonsterInit();

	// 엘리트 몬스터 타입 설정 기본적으로 해주는거인데 공격 타입 제한을 걸어버리는 거랑 다름없어서
	// 블프에서 엘리트 몬스터 타입 바꿔버리고 싶으면 여기 내용 지우세요
	switch (EliteType)
	{
	case EEliteMonsterType::Elite1:
	{
		AttackType = EMonsterAttackType::Melee;
		break;
	}
	case EEliteMonsterType::Elite2:
	{
		AttackType = EMonsterAttackType::Ranged;
	}
	case EEliteMonsterType::Elite3:
	{
		AttackType = EMonsterAttackType::Hybrid;
	}
	}
}

EEliteMonsterType AEliteMonster::GetEliteType() const
{
	return EliteType;
}
