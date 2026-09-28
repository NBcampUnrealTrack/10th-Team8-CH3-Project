#include "EliteMonster.h"
#include "CombatStatsComponent.h"
#include "Components/CapsuleComponent.h"
#include "EliteMonsterAIController.h"
#include "MonsterSkill.h"
#include "Components/DecalComponent.h"

AEliteMonster::AEliteMonster()
{
	MonsterCapsuleCollisionComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule Collision"));
	
	//이거 없으면 충돌체 적중 안됨
	MonsterCapsuleCollisionComponent->SetCollisionProfileName(TEXT("Pawn"));

	MonsterCollisionComponent = MonsterCapsuleCollisionComponent;
	MonsterCollisionComponent->SetupAttachment(RootComponent);

	AIControllerClass = AEliteMonsterAIController::StaticClass();

	EliteType = EEliteMonsterType::Elite1; //일단 기본은 Elite1

	AttackType = EMonsterAttackType::Melee;

	// 엘리트 체력 기본값. 바꿀거면 하세용
	//등급 배율 2.2가 여기에 또 곱해지므로 잡몹의 3배로 두면 실효 6.6배가 되어버림
	//120~180으로 두면 L1 쉬움 실효 264~396 잡몹의 약 5배로 맞음
	MinHealthRadius = 120.0f;
	MaxHealthRadius = 180.0f;

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

}

EEliteMonsterType AEliteMonster::GetEliteType() const
{
	return EliteType;
}
