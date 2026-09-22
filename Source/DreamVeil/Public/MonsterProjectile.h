// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MonsterProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UPrimitiveComponent;

UCLASS()
class DREAMVEIL_API AMonsterProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	AMonsterProjectile();
	
	void SetDamage(float damage);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile")
	TObjectPtr<USphereComponent> BulletCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> BulletMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> Movement;

private:
	UFUNCTION()
	void HandleHit(
		UPrimitiveComponent* HitComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit
	);

	float Damage;
	bool bHitProcessed = false;
};
