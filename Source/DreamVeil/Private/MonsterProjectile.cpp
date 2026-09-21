// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterProjectile.h"
#include "Components/SphereComponent.h"

// Sets default values
AMonsterProjectile::AMonsterProjectile()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	BulletCollision = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere Collision"));

	SetRootComponent(BulletCollision);
	BulletCollision->InitSphereRadius(10.0f); // 기본값으로 좀 작게만들기

	//Details 패널에서 설정 가능한 Collision 관련 설정 코드에서 해주기
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

}

void AMonsterProjectile::SetDamage(float damage)
{
	// 0보단 커야지
	this->Damage = FMath::Max(0.0f, damage);
}

// Called when the game starts or when spawned
void AMonsterProjectile::BeginPlay()
{
	Super::BeginPlay();
	
}

void AMonsterProjectile::HandleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
}

// Called every frame


