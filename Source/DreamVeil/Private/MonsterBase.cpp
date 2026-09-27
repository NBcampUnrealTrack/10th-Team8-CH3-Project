// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterBase.h"
#include "MonsterSkill.h"
#include "MonsterAIController.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
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
	MonsterSkill = CreateDefaultSubobject<UMonsterSkill>(TEXT("MonsterSkill"));
	MonsterCollisionComponent = nullptr;
	MonsterMeshComponent = GetMesh();

	// 스킬이나 기본 공격의 범위를 나타내어 경고하는 영역. 데칼컴포넌트로 구현한다.
	// 일단 엘리트 몬스터만 사용할 거긴 한데 근접 몬스터에서 쓰면 좋을듯? 몰?루
	AttackWarningEffect = CreateDefaultSubobject<UDecalComponent>(TEXT("MonsterAttackWarning"));
	AttackWarningEffect->SetupAttachment(RootComponent);
	// 부모 몬스터의 회전 등과 관계 없이 월드 좌표에 경고 이펙트 고정하기.
	AttackWarningEffect->SetAbsolute(true, true, true);
	// 화면에서 작게 보인다고 경고가 사라지지 않도록 설정
	AttackWarningEffect->SetFadeScreenSize(0.0f);
	AttackWarningEffect->SetVisibility(false);

	//머리 판정용 구
	//실제 충돌을 꺼두는 이유 이동용 캡슐이 머리까지 감싸고 있어서 켜도 총알 광선이 여기까지 닿지 못하고 물리에만 방해가 됨
	//대신 총알 광선이 이 구를 지나갔는지로 헤드샷을 판정함 IsHeadshotHit 참고
	HeadCollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("HeadCollision"));
	HeadCollisionComponent->SetupAttachment(GetMesh(), HeadSocketName);
	HeadCollisionComponent->SetSphereRadius(20.0f);
	HeadCollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadCollisionComponent->SetCanEverAffectNavigation(false);

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


void AMonsterBase::ShowAttackWarning(const FVector& StartPos, const FVector& EndPos, float AttackWidth)
{
	FVector Direction = EndPos - StartPos;
	Direction.Z = 0.0f;

	// 시작지점에서 끝부분까지 거리만 가져오기. 방향은 없수
	const float AttackLength = Direction.Size();

	// KINDA_SMALL_NUMBER라는게 왜있냐... 이거 일단 0.00001f
	//정확히는 0.0001f(1.e-4f)임 0이 하나 많았음 UnrealMathUtility.h:130 << 아 그렇군요 사실 대충친거였음
	//float는 계산을 거치면 0이어야 할 값이 0.0000000437 같은 쓰레기로 남아서 == 0.0f 비교를 믿을 수 없음
	//그래서 "이 정도면 0으로 치자"는 기준선이 필요한 것 여기서는 공격 범위 길이가 0이면 그릴 데칼이 없다는 뜻
	//UE_를 붙인 이유 UE 5.6에서 UE_ 없는 이름은 deprecated라 그냥 쓰면 경고가 나고 우리는 -WarningsAsErrors로 빌드함
	if (AttackLength <= UE_KINDA_SMALL_NUMBER || AttackWidth <= 0.0f)
	{
		HideAttackWarning();
		return;
	}

	// 이렇게 GetSafeNormal을 사용하지 않고 직접 나눠 방향벡터 정규화하는데, 이유는 각자 쓸 곳이 있기 때문이다.
	Direction /= AttackLength;

	FVector Center = StartPos + Direction * (AttackLength * 0.5f);
	Center.Z = StartPos.Z;

	const float DirectionYaw = Direction.Rotation().Yaw;

	// 직사각형 각도를 지정해줌.
	// 데칼 투영은 바닥에 투영해야하므로 -90.0f로 바닥을 바라보게 해주고,
	// DirectionYaw로 공격 방향을 직사각형이 길게 가리키도록 회전시킨다.
	AttackWarningEffect->SetWorldLocationAndRotation(
		Center, FRotator(-90.0f, DirectionYaw, 0.0f)
	);

	// 데칼은 사이즈가 DecalSize로 정해지기에, 혹시라도 컴포넌트 자체에 있는 Scale값이 수정되어있을 걸 방지해서
	// 스케일 값을 1로 초기화해주는 과정. OneVector = 1 1 1임
	AttackWarningEffect->SetWorldScale3D(FVector::OneVector);

	// 데칼 사이즈를 정해준다. 100은 투영 깊이, 나머지는 투영 범위 마즘
	AttackWarningEffect->DecalSize = FVector(100.0f, AttackWidth * 0.5f, AttackLength * 0.5f);

	// 데칼 설정이 변경되었으니, 렌더링을 다시 갱신해야 적용되기에 MakrkRenderStateDirty로
	// 갱신이 필요한 상태라고 저장을 해 두면, 엔진에서 이 상태를 보고 렌더링을 다시 갱신해준다.
	AttackWarningEffect->MarkRenderStateDirty();
	AttackWarningEffect->SetVisibility(true);
}

void AMonsterBase::HideAttackWarning()
{
	AttackWarningEffect->SetVisibility(false);
}


void AMonsterBase::BeginPlay()
{
	Super::BeginPlay();
	// 여기서 자식클래스가 재구성한 MonsterInit이 호출될거니 나머지 BeginPlay에선 호출 ㄴㄴ
	MonsterInit();
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCanEverAffectNavigation(false);
	//서로 피해 가기(RVO)를 켬 끄면 몬스터들이 플레이어까지 최단 경로 하나에 전부 몰려서
	//한 줄로 줄지어 오거나 한 지점에서 서로 밀며 겹쳐 보임 캡슐끼리 막기만으로는 이게 안 풀림
	//막기는 이미 서로 통과하지 못하게만 해주고 길을 비켜주지는 않기 때문
	GetCharacterMovement()->SetAvoidanceEnabled(true);

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

	//머리 구를 머리 뼈에 다시 붙임 블루프린트에서 소켓 이름을 바꿨거나 메시를 갈아 끼웠어도 따라가게 함
	//소켓이 없는 메시(스태틱 메시 몬스터)면 붙이지 않고 블루프린트에 잡아둔 위치를 그대로 씀
	if (IsValid(HeadCollisionComponent) && IsValid(GetMonsterMesh()) && GetMonsterMesh()->DoesSocketExist(HeadSocketName))
	{
		HeadCollisionComponent->AttachToComponent(GetMonsterMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, HeadSocketName);
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
	if (MonsterSkill && MonsterSkill->IsUsingSkill()) return false;
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
	if (MonsterSkill) MonsterSkill->CancelSkill();
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

void AMonsterBase::BeginRagdoll(const FVector& LaunchVelocity)
{
	// 이미 레그돌이거나, 몬스터 스탯이 존재하지 않거나, 죽었으면 나가라
	if (bIsRagdoll || !MonsterCombatStats || MonsterCombatStats->IsDead()) return;

	// 캐싱 시도, 스켈레탈 안 쓰는 친구면 여기서 실패할듯
	USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(GetMesh());

	// 스켈레탈 메시 없는 친구나 아니면 물리 없는 친구는 나가라
	if (!SkeletalMesh || !SkeletalMesh->GetPhysicsAsset()) return;

	// 레그돌 시작여부
	bIsRagdoll = true;

	if (AMonsterAIController* AIController = Cast<AMonsterAIController>(this->GetController()))
	{
		// BT에 레그돌 상태 전달하기
		AIController->SetRagdollState(true);
	}
	// 하던 공격 전부 중단하기
	CancelAttack();

	// 걷고 있었다면 적용되던 가속도 전부 지우고, 이동기능 꺼버리기
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// Character Class 기본으로 들어있는 캡슐 컴포넌트만 플레이어랑 충돌하니까 그거 꺼버리는거임
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 피격 콜리전도 꺼버릴까 해서 일단 여따 적어둡니다
	if (IsValid(MonsterCollisionComponent))
	{
		MonsterCollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	SkeletalMesh->SetCollisionObjectType(ECC_PhysicsBody);
	SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	SkeletalMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	SkeletalMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
}
EMonsterAttackType AMonsterBase::GetAttackType() const
{
	return AttackType;
}

UMeshComponent* AMonsterBase::GetMonsterMesh() const
{
	return MonsterMeshComponent;
}

//이 총알이 머리를 지나갔는지
bool AMonsterBase::IsHeadshotHit(const FHitResult& HitResult) const
{
	if (!IsValid(HeadCollisionComponent))
	{
		return false;
	}

	//맞은 컴포넌트가 머리인지로 판단하지 않는 이유
	//이동용 캡슐이 머리끝까지 감싸고 있어서 광선은 언제나 캡슐 표면에서 먼저 막힘 그래서 머리 구는 절대 맞은 컴포넌트가 될 수 없음
	//대신 총알이 지나간 선(TraceStart~TraceEnd)과 머리 중심의 거리를 재서 구를 스쳐 갔는지를 봄
	//이러면 캡슐 어디에 맞았는지와 상관없이 머리를 겨눴는지로 판정됨
	const FVector HeadCenter = HeadCollisionComponent->GetComponentLocation();
	const float HeadRadius = HeadCollisionComponent->GetScaledSphereRadius();

	return FMath::PointDistToSegment(HeadCenter, HitResult.TraceStart, HitResult.TraceEnd) <= HeadRadius;
}

void AMonsterBase::OnDeath()
{
	CancelAttack();
	HideAttackWarning();
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
