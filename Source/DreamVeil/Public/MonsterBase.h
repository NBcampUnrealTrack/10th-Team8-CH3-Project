// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MonsterType.h"
#include "TimerManager.h"
#include "MonsterBase.generated.h"

class AMonsterProjectile;
class UAnimMontage;
class UShapeComponent;
class UCombatStatsComponent;
class UDispatchTableComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOnMonsterAttackFinished,
	bool
);

UCLASS()
class DREAMVEIL_API AMonsterBase : public ACharacter
{
	GENERATED_BODY()
public:
	AMonsterBase();

	float GetMonsterAttackRange() const;

	//하단 세 함수는 공격 관련인데 외부에서 호출해야 될 수도 있어서 public으로 둠
	// 공격 시작 성공 여부
	bool StartAttack(AActor* target);
	
	// Anim Notify에서 호출할 예정
	void ExecuteAttack();

	// 얜 기절이나 죽었을 때 중단시키는 용도
	void CancelAttack();

	FOnMonsterAttackFinished OnAttackFinished;

	//받은 데미지를 증강 라이브러리로 넘김 이게 없으면 체력이 안 깎임
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	//스탯 컴포넌트 체력 공격력 방어력과 사망 이벤트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	TObjectPtr<UCombatStatsComponent> MonsterCombatStats;

	//증강 컴포넌트 타입별 패시브와 웨이브 강화
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Augment")
	TObjectPtr<UDispatchTableComponent> MonsterDispatchTable;

	// 이게 실제 메쉬 컴포넌트 이야기. skeletal 버리고 쓸거 상정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TObjectPtr<UMeshComponent> MonsterMeshComponent;
	
	// 이속
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MonsterWalkSpeed;

	// 몬스터가 원거리인지 근거리인지 체크
	UFUNCTION(BlueprintPure, Category = "Monster|Attack")
	EMonsterAttackType GetAttackType() const;

	// GetMesh대신 이거 써주세요.
	UFUNCTION(BlueprintPure, Category= "Monster|Mesh")
	UMeshComponent* GetMonsterMesh() const;

	UFUNCTION()
	virtual void OnDeath();
protected:
	virtual void BeginPlay() override;
	virtual void MonsterInit();

	// 얘가 근거리인지 원거리인지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster|Attack")
	EMonsterAttackType AttackType;

	// 근접공격 시점
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster|Attack")
	TObjectPtr<UAnimMontage> MeleeAttackMontage;

	// 원거리 공격 시점
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster|Attack")
	TObjectPtr<UAnimMontage> RangedAttackMontage;

	// 투사체 오브젝트 클래스 타입.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster|Attack|Ranged")
	TSubclassOf<AMonsterProjectile> RangedProjectile;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster|Attack|Ranged")
	FVector ProjectileSpawnOffset = FVector(100.0f, 0.0f, 30.0f);

	// 추격을 멈추고 공격을 실행할 거리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Attack")
	float MonsterAttackRange;

	// 근접공격의 범위
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Attack")
	float MeleeAttackRadius;

	// 애니메이션에서 사용할 공격 기점 몽타주
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	//공격 기본값. 공격거리 수정 필요하면 여서 하세요
	const float MELEE_ATTACK_RADIUS_BASE = 100.0f;
	const float RANGED_ATTACK_RADIUS_BASE = 2000.0f;
	const float HYBRID_ATTACK_RADIUS_BASE = 1000.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	TObjectPtr<UShapeComponent> MonsterCollisionComponent;

	// 체력 컴포넌트에 저장될 수치, 얘는 읽기만 가능
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats|Health")
	float MaxHealth;

	// 이 두 가지 범위중에서 랜덤하게 자동생성될 예정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|HealthRadius")
	float MinHealthRadius;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|HealthRadius")
	float MaxHealthRadius;

private:
	void PerformMeleeCheck();
	void SpawnAttackProjectile();

	void HandleAttackMontageEnded(
		UAnimMontage* Montage,
		bool bInterrupted
	);

	void FinishAttack(bool bSucceeded);

	bool bIsAttacking = false;
	bool bAttackExecuted = false;

	TWeakObjectPtr<AActor> AttackTarget;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveAttackMontage;

	FTimerHandle AttackHitTimer;
	FTimerHandle AttackEndTimer;
};
