// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterBase.h"
#include "MonsterAIController.h"
#include "Components/SphereComponent.h"
#include "CombatStatsComponent.h"
#include "DispatchTableComponent.h"
#include "AugmentDamageLibrary.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"

// Sets default values
AMonsterBase::AMonsterBase()
{
	PrimaryActorTick.bCanEverTick = false;
	// 월드에 스폰됐을 경우 AIController Possess 시키기
	AutoPossessAI = EAutoPossessAI::PlacedInWorld;
	MonsterCombatStats = CreateDefaultSubobject<UCombatStatsComponent>(TEXT("MonsterCombatStats"));
	MonsterDispatchTable = CreateDefaultSubobject<UDispatchTableComponent>(TEXT("MonsterDispatchTable"));
	MonsterCollisionComponent = nullptr;
	MonsterMeshComponent = GetMesh();

	AttackType = EMonsterAttackType::Melee; //기본적으로 근접, BP에서 설정 가능


	// 체력 기본값. 에러방지용이라 수정하셈
	MinHealthRadius = 100.0f;
	MaxHealthRadius = 150.0f;

	//이동속도
	MonsterWalkSpeed = 500.0f;
	// 추격을 멈추고 공격할 수 있는 거리, 기본값은 100이지만, 
	// 자식 생성자 마지막과 BeginPlay에서 갱신할거임. 일단 안전용
	MonsterAttackRange = 100.0f;

	// 아래 세 줄 코드는 NavMesh를 이동할 때 플레이어만 바라보고 오는 게 아닌, 
	// NavMesh에 따라 경로를 바라보고 오게 자연스럽게 보이기 위함
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;

	// AI들끼리 줄지어 오는 것이 아닌, 서로 피하면서 추격할 수 있게 하는 코드들.
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 360.0f, 0.0f);
	GetCharacterMovement()->bUseRVOAvoidance = true;
	GetCharacterMovement()->AvoidanceConsiderationRadius = 500.0f;

}



// Called when the game starts or when spawned
void AMonsterBase::BeginPlay()
{
	Super::BeginPlay();
	// 여기서 자식클래스가 재구성한 MonsterInit이 호출될거니 나머지 BeginPlay에선 호출 ㄴㄴ
	MonsterInit();
	
}

//몬스터 초기화 작업. 몬스터가 전장에 투입될 때 스탯 초기화 등
void AMonsterBase::MonsterInit()
{
	//이속 
	GetCharacterMovement()->MaxWalkSpeed = MonsterWalkSpeed;
	// 최대체력인디.. 범위 변수는 블루프린트에서 ㄱㄱ
	this->MaxHealth = FMath::RandRange(MinHealthRadius, MaxHealthRadius);
	MonsterCombatStats->SetMaxHealth(MaxHealth);
	// 어택타입 보기
	switch (AttackType)
	{
	case EMonsterAttackType::Melee:
	{
		MonsterAttackRange = MELEE_ATTACK_RADIUS_BASE;
		break;
	}
	case EMonsterAttackType::Ranged:
	{
		MonsterAttackRange = RANGED_ATTACK_RADIUS_BASE;
		break;
	}
	case EMonsterAttackType::Hybrid:
	{
		MonsterAttackRange = HYBRID_ATTACK_RADIUS_BASE;
		break;
	}
	default:
	{
		AttackType = EMonsterAttackType::Melee;
		MonsterAttackRange = MELEE_ATTACK_RADIUS_BASE;
	}
	}

}

float AMonsterBase::GetMonsterAttackRange() const
{
	return MonsterAttackRange;
}

EMonsterAttackType AMonsterBase::GetAttackType() const
{
	return AttackType;
}

UMeshComponent* AMonsterBase::GetMonsterMesh() const
{
	return MonsterMeshComponent;
}

//받은 데미지를 증강 라이브러리로 넘김 방어력 체력 흡혈 가시 갑옷 처리
float AMonsterBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Damage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (Damage <= 0.0f)
	{
		return 0.0f;
	}

	return UAugmentDamageLibrary::ProcessIncomingDamage(this, Damage, DamageEvent.DamageTypeClass, EventInstigator, DamageCauser);
}
void Death(APawn* Pawn)
{

}