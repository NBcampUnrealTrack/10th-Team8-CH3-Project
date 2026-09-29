	//Details 패널에서 설정 가능한 Collision 관련 설정 코드에서 해주기
// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/DamageType.h"
#include "MonsterBase.h"

// Sets default values
AMonsterProjectile::AMonsterProjectile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	BulletCollision = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere Collision"));

	SetRootComponent(BulletCollision);
	BulletCollision->InitSphereRadius(10.0f); // 기본값으로 좀 작게만들기

	//Collision 관련 설정 정해주기. 특정 채널은 통과하고, 특정 채널은 막히도록 설정하는 것들...

	BulletCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BulletCollision->SetCollisionObjectType(ECC_WorldDynamic);
	BulletCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	BulletCollision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	BulletCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	BulletCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	BulletCollision->OnComponentHit.AddDynamic(
		this,
		&AMonsterProjectile::HandleHit
	);

	BulletMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BulletMesh"));

	BulletMesh->SetupAttachment(BulletCollision);
	BulletMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));

	Movement->SetUpdatedComponent(BulletCollision);
	Movement->InitialSpeed = 1500.0f;
	Movement->MaxSpeed = 1500.0f;
	Movement->Velocity = FVector(1.0f, 0.0f, 0.0f);
	Movement->bInitialVelocityInLocalSpace = true;
	Movement->bRotationFollowsVelocity = true;
	Movement->ProjectileGravityScale = 0.0f;

	InitialLifeSpan = 5.0f;
}

void AMonsterProjectile::SetDamage(float damage)
{
	// 0보단 커야지
	this->Damage = FMath::Max(0.0f, damage);
}

void AMonsterProjectile::IgnoreActorWhileMoving(AActor* OtherActor)
{
	if (IsValid(OtherActor) && BulletCollision)
	{
		BulletCollision->IgnoreActorWhenMoving(OtherActor, true);
	}
}

// Called when the game starts or when spawned
void AMonsterProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	if (AActor* OwnerActor = GetOwner())
	{
		BulletCollision->IgnoreActorWhenMoving(OwnerActor, true);
	}

	if (APawn* InstigatorPawn = GetInstigator())
	{
		BulletCollision->IgnoreActorWhenMoving(InstigatorPawn, true);
	}
}

void AMonsterProjectile::HandleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bHitProcessed ||
		!IsValid(OtherActor) ||
		OtherActor == this ||
		OtherActor == GetOwner() ||
		OtherActor == GetInstigator())
	{
		return;
	}

	bHitProcessed = true;

	ProcessHit(OtherActor, Hit);

	Destroy();
}

void AMonsterProjectile::ProcessHit(AActor* OtherActor, const FHitResult& Hit)
{
	// 예제에서는 Pawn에만 피해 적용
	// 아근데이거 고장나려나 테스트필요
	if (Cast<APawn>(OtherActor) && !Cast<AMonsterBase>(OtherActor))
	{
		UGameplayStatics::ApplyDamage(
			OtherActor,
			Damage,
			GetInstigatorController(),
			this,
			UDamageType::StaticClass());
	}
}


