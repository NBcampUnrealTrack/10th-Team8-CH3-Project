// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterBase.h"
#include "MonsterAIController.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "CombatStatsComponent.h"
#include "DispatchTableComponent.h"
#include "AugmentDamageLibrary.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "MonsterProjectile.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "MainPlayerCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MonsterCollision.h"

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

	// 뎀지. 수정할거임
	MonsterDamage = 10.0f;

	//이동속도
	MinWalkSpeed = 500.0f;
	MaxWalkSpeed = 1000.0f;
	// 추격을 멈추고 공격할 수 있는 거리, 기본값은 200이지만, 
	// 자식 생성자 마지막과 BeginPlay에서 갱신할거임. 일단 안전용
	MonsterAttackRange = 200.0f;

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

void AMonsterBase::BeginPlay()
{
	Super::BeginPlay();
	// 여기서 자식클래스가 재구성한 MonsterInit이 호출될거니 나머지 BeginPlay에선 호출 ㄴㄴ
	MonsterInit();
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCanEverAffectNavigation(false);
	GetCharacterMovement()->SetAvoidanceEnabled(false);

	if (MonsterCombatStats)
	{
		MonsterCombatStats->OnDead.AddDynamic(
			this,
			&AMonsterBase::OnDeath
		);
	}
	
}

//몬스터 초기화 작업. 몬스터가 전장에 투입될 때 스탯 초기화 등
void AMonsterBase::MonsterInit()
{
#pragma region Collisions
	UCapsuleComponent* MovementCapsule = GetCapsuleComponent();

	MovementCapsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MovementCapsule->SetCollisionObjectType(MonsterCollision::Monster);
	MovementCapsule->SetCollisionResponseToAllChannels(ECR_Block);
	MovementCapsule->SetCollisionResponseToChannel(MonsterCollision::MonsterProjectile, ECR_Ignore);
	MovementCapsule->SetCollisionResponseToChannel(MonsterCollision::MonsterHitbox, ECR_Ignore);
	MovementCapsule->SetCanEverAffectNavigation(false);
	
	if (IsValid(MonsterCollisionComponent))
	{
		MonsterCollisionComponent->SetSimulatePhysics(false);
		MonsterCollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		MonsterCollisionComponent->SetCollisionObjectType(MonsterCollision::MonsterHitbox);
		MonsterCollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		MonsterCollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		MonsterCollisionComponent->SetGenerateOverlapEvents(false);
		MonsterCollisionComponent->SetCanEverAffectNavigation(false);
	}

#pragma endregion

	//이속
	this->MonsterWalkSpeed = FMath::RandRange(MinWalkSpeed, MaxWalkSpeed);
	GetCharacterMovement()->MaxWalkSpeed = MonsterWalkSpeed;
	// 최대체력인디.. 범위 변수는 블루프린트에서 ㄱㄱ
	this->MaxHealth = FMath::RandRange(MinHealthRadius, MaxHealthRadius);

	if (IsValid(MonsterCombatStats))
	{
		MonsterCombatStats->InitStats(MaxHealth, 1.0f, MonsterDamage);
	}
	// 어택타입 보기
	switch (GetAttackType())
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

void AMonsterBase::PerformMeleeCheck()
{
	// 적 기준 전방에 타격범위 구체 생성. 몬스터 기준 반경이 아니라 몬스터 전방에 반경이 있는 것.
	const FVector HitCenter = GetActorLocation() + GetActorForwardVector() * MeleeAttackRadius;

	// 충돌 검색 오브젝트의 조건 걸기
	FCollisionObjectQueryParams ObjectParameters;
	// Pawn만 검색할것임.
	ObjectParameters.AddObjectTypesToQuery(ECC_Pawn);

	// 충돌 제외 설정 걸기
	FCollisionQueryParams QueryParameters;
	// 난 뺄거임
	QueryParameters.AddIgnoredActor(this);

	// 결과값 저장할 곳
	TArray<FOverlapResult> OverlapResults;

	// 오버랩 충돌체크 실행.
	GetWorld()->OverlapMultiByObjectType(
		OverlapResults,		// 결과값
		HitCenter,			// 오버랩 시작 위치
		FQuat::Identity,	// 검사 영역 회전인데 필요없어서 걍 이렇게
		ObjectParameters,	// 난 Pawn만 검색할거임
		FCollisionShape::MakeSphere(MeleeAttackRadius),	//즉석으로 설정 반경만큼 구 만들어서 충돌체크
		QueryParameters		// 근데 난 빼주셈
	);

	// 난 지금부터 감지된 것들을 하나씩 까볼겨
	for (const FOverlapResult& Result : OverlapResults)
	{
		// 님 플레이어임?
		AMainPlayerCharacter* ResultActor = Cast<AMainPlayerCharacter>(Result.GetActor());
		// 아니면 비켜 방해된다
		if (!IsValid(ResultActor)) return;

		//플레이어면 데미지 준다잇
		UGameplayStatics::ApplyDamage(
			ResultActor,
			MonsterCombatStats->GetAttackPower(),
			GetController(),
			this,
			UDamageType::StaticClass()
		);
		//플레이어는 하나니까 뎀 줬으면 이 반복문 종료
		break;
	}
}

void AMonsterBase::SpawnAttackProjectile()
{
	// 타겟 가져오고 체크
	AActor* Target = AttackTarget.Get();
	if (!IsValid(Target) || !RangedProjectile) return;

	// 원거리 공격이 어디서 스폰될지 정해줌
	const FVector SpawnLocation = GetActorLocation() + GetActorRotation().RotateVector(ProjectileSpawnOffset);
	
	// 정규화로 방향벡터 구하기
	const FVector Dir = (Target->GetActorLocation() - SpawnLocation).GetSafeNormal();

	// Transform으로 묶기
	const FTransform SpawnTransform(Dir.Rotation(), SpawnLocation);

	//
	AMonsterProjectile* Projectile = 
		GetWorld()->SpawnActorDeferred<AMonsterProjectile>(
			RangedProjectile,
			SpawnTransform,
			this,
			this,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn
		);

	if (!Projectile) return;

	Projectile->SetDamage(MonsterCombatStats->GetAttackPower());

	UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
}

float AMonsterBase::GetMonsterAttackRange() const
{
	return MonsterAttackRange;
}

// 이 함수에 변수명이 겹칠 사항들이 많이 보여서 구분을 위해 겹칠만한 변수명 앞에 다 Temp붙여뒀음.
bool AMonsterBase::StartAttack(AActor* Target)
{
	// 공격 조건 충족 여부 확인
	if (bIsAttacking || !IsValid(Target) || Target == this) return false;
	
	// 타입이 안 정해진 놈이면 비켜라
	const EMonsterAttackType TempAttackType = GetAttackType();
	if (TempAttackType != EMonsterAttackType::Melee
		&& TempAttackType != EMonsterAttackType::Ranged
		&& TempAttackType != EMonsterAttackType::Hybrid)
	{
		return false;
	}

	// 원거리 몬스터인데 원거리 투사체 캐싱 안됐으면 실패
	if (TempAttackType == EMonsterAttackType::Ranged && !RangedProjectile) return false;

	FVector Dir = Target->GetActorLocation() - GetActorLocation();
	Dir.Z = 0.0f;

	if (!Dir.IsNearlyZero())
	{
		SetActorRotation(Dir.Rotation());
	}

	AttackTarget = Target;
	bAttackExecuted = false;
	bIsAttacking = true;
	ActiveAttackMontage = nullptr;

	if (USkeletalMeshComponent* TempSkeletalMesh = Cast<USkeletalMeshComponent>(GetMonsterMesh()))
	{
		UAnimInstance* TempAnimInstance = TempSkeletalMesh->GetAnimInstance();

		UAnimMontage* TempAnimMontage = TempAttackType == EMonsterAttackType::Melee
			? MeleeAttackMontage.Get()
			: TempAttackType == EMonsterAttackType::Ranged
			? RangedAttackMontage.Get()
			/* 젠장 하이브리드가 존재하질 않아. . . .. . . .. . .
			: TempAttackType == EMonsterAttackType::Hybrid
			? HybridAttackMontage.Get()
			*/
			: nullptr;

		if (!TempAnimInstance || !TempAnimMontage || TempAnimMontage == nullptr)
		{
			bIsAttacking = false;
			AttackTarget.Reset();
			return false;
		}
			
		ActiveAttackMontage = TempAnimMontage;

		if (TempAnimInstance->Montage_Play(TempAnimMontage) <= 0.0f)
		{
			bIsAttacking = false;
			ActiveAttackMontage = nullptr;
			AttackTarget.Reset();
			return false;
		}
		
		// 몽타주 정상종료, 중단 등 모두 콜백으로 전달시키기
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(
			this,
			&AMonsterBase::HandleAttackMontageEnded
		);
		// 엔드 델리게이트 세팅
		TempAnimInstance->Montage_SetEndDelegate(EndDelegate, TempAnimMontage);

		return true;
	}
	// Static Mesh 안쓸거 같아서 구현은 안 해두는데 이거 만약 쓰면 여따 구현내용 남겨주세요

	bIsAttacking = false;
	AttackTarget.Reset();
	return false;
}

// 공격판정 여기서 
void AMonsterBase::ExecuteAttack()
{
	if (!bIsAttacking || bAttackExecuted) return;

	bAttackExecuted = true;

	switch (GetAttackType())
	{
	case EMonsterAttackType::Melee:
	{
		PerformMeleeCheck();
		break;
	}
	case EMonsterAttackType::Ranged:
	{
		SpawnAttackProjectile();
		break;
	}
	case EMonsterAttackType::Hybrid:
	{
		//하이브리드 구현 아직 보류
		break;
	}
	default:
	{ break; }
	}
}

void AMonsterBase::HandleAttackMontageEnded(
	UAnimMontage* Montage,
	bool bInterrupted)
{
	if (!bIsAttacking || Montage != ActiveAttackMontage.Get())
	{
		return;
	}

	FinishAttack(!bInterrupted);
}

void AMonsterBase::FinishAttack(bool bSucceeded)
{
	if (!bIsAttacking)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(AttackHitTimer);
	GetWorldTimerManager().ClearTimer(AttackEndTimer);

	bIsAttacking = false;
	bAttackExecuted = false;
	AttackTarget.Reset();
	ActiveAttackMontage = nullptr;

	OnAttackFinished.Broadcast(bSucceeded);
}

void AMonsterBase::CancelAttack()
{
	if (!bIsAttacking)
	{
		return;
	}

	bAttackExecuted = true;

	if (USkeletalMeshComponent* SkeletalMesh =
		Cast<USkeletalMeshComponent>(GetMonsterMesh()))
	{
		if (UAnimInstance* AnimInstance = SkeletalMesh->GetAnimInstance())
		{
			if (UAnimMontage* Montage = ActiveAttackMontage.Get())
			{
				FOnMontageEnded EmptyDelegate;
				AnimInstance->Montage_SetEndDelegate(
					EmptyDelegate,
					Montage);

				AnimInstance->Montage_Stop(0.1f, Montage);
			}
		}
	}

	FinishAttack(false);
}

EMonsterAttackType AMonsterBase::GetAttackType() const
{
	return AttackType;
}

UMeshComponent* AMonsterBase::GetMonsterMesh() const
{
	return MonsterMeshComponent;
}

void AMonsterBase::OnDeath()
{
	Destroy();
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