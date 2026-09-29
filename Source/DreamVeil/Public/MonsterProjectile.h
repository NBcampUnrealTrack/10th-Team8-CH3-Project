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

	// 같이 발사된 투사체끼리 부딪혀 사라지지 않도록 서로 무시시킬 때 사용
	void IgnoreActorWhileMoving(AActor* OtherActor);

protected:
	virtual void BeginPlay() override;

	// 무언가에 맞았을 때의 처리. 기본은 맞은 폰(플레이어)에게만 직격 피해
	// 자식 클래스(폭발탄 등)가 덮어써서 맞았을 때의 동작을 바꿈. 호출 뒤 투사체는 바로 파괴됨
	virtual void ProcessHit(AActor* OtherActor, const FHitResult& Hit);

	float GetDamage() const { return Damage; }

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
