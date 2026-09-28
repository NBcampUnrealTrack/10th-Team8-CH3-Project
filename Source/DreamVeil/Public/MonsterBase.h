// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MonsterType.h"
#include "TimerManager.h"
#include "Engine/HitResult.h"
#include "MonsterBase.generated.h"

class AMonsterProjectile;
class UAnimMontage;
class UShapeComponent;
class USphereComponent;
class UCombatStatsComponent;
class UDispatchTableComponent;
class UDecalComponent;
class UMonsterSkill;
class UMaterialInterface;
class UParticleSystem;

//사망 연출과 별개로 게임모드에 처치 완료를 알림. 보스는 사망 5초 뒤 전달함
DECLARE_MULTICAST_DELEGATE(FOnMonsterDeathReported);

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
	//BT는 부모 타입으로 호출하므로 보스의 스킬 우선 판단도 실행되도록 가상 함수로 둠
	virtual bool StartAttack(AActor* target);
	
	// Anim Notify에서 호출할 예정
	void ExecuteAttack();

	// 얜 기절이나 죽었을 때 중단시키는 용도
	void CancelAttack();

	FOnMonsterAttackFinished OnAttackFinished;

	//게임모드는 체력 컴포넌트의 즉시 사망 이벤트 대신 이 신호로 처치 수와 클리어를 처리함
	FOnMonsterDeathReported OnDeathReported;

	// 날릴 속도를 입력값으로 받아서 레그돌로 만들어서 날려버리깅
	UFUNCTION(BlueprintCallable, Category="Monster|Ragdoll")
	void BeginRagdoll(const FVector& LaunchVelocity);

	// 레그돌 상태이거나 일어나는 중이면 true
	UFUNCTION(BlueprintPure, Category = "Monster|Ragdoll")
	bool IsRagdoll() const { return bIsRagdoll; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Monster|Skill")
	TObjectPtr<UMonsterSkill> MonsterSkill;

	bool IsAttacking() const { return bIsAttacking; }

	// 시각 효과는 몬스터가 소유하고, 스킬 컴포넌트에서 필요할 때 요청한다.
	void ShowAttackWarning(const FVector& StartPos, const FVector& EndPos, float AttackWidth);
	void HideAttackWarning();

	// 스킬이 경고 데칼을 추가로 만들 때 같은 머티리얼을 쓰기 위함
	UMaterialInterface* GetAttackWarningMaterial() const;

	// 스킬에서 몬스터의 기본 투사체 설정을 재사용하기 위함
	TSubclassOf<AMonsterProjectile> GetRangedProjectileClass() const { return RangedProjectile; }

	// 지금 공격력이 기본 공격력(MonsterDamage)의 몇 배인지
	// 난이도 레벨 등급 배율(제곱근)과 공격력 증강이 모두 들어 있음 스킬 피해를 평타와 같은 비율로 키울 때 씀
	float GetAttackPowerScale() const;

	// 스킬 피해 배율 쉬움 L1에서 1이 되도록 등급 배율을 뺀 값
	// 스킬마다 적어둔 피해(돌진 30 부채꼴 15 장판 5)가 쉬움 L1에서는 그대로 들어가고
	// 난이도 레벨이 오르거나 공격력 증강을 얻으면 평타와 같은 비율로 커짐
	// 엘리트와 보스가 같은 수치를 써도 쉬움 L1에서 같은 피해가 나오게 하려고 등급은 빼둠
	float GetSkillDamageScale() const;
	FVector GetProjectileSpawnOffset() const { return ProjectileSpawnOffset; }

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

	//머리 판정용 구 헤드샷을 어디까지 인정할지 눈으로 보고 조절하라고 컴포넌트로 둠
	//블루프린트에서 Sphere Radius와 위치를 바꾸면 그대로 판정 범위가 바뀜
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collision")
	TObjectPtr<USphereComponent> HeadCollisionComponent;

	//이 총알이 머리를 지나갔는지 헤드샷이면 데미지가 올라감
	//머리 판정을 몬스터가 직접 하는 이유 머리 구의 위치와 크기를 아는 것은 몬스터 자신뿐임
	//총 적중(UAugmentDamageLibrary::ApplyWeaponHit)만 이걸 물어봄 범위 공격 화염 가시 갑옷은 머리 판정을 하지 않음
	UFUNCTION(BlueprintPure, Category = "Collision")
	bool IsHeadshotHit(const FHitResult& HitResult) const;

	UFUNCTION()
	virtual void OnDeath();
protected:
	virtual void BeginPlay() override;
	//보스가 체력 사망 외의 경로로 사라지는 원인을 확인하기 위해 종료 시점의 상태를 기록함
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void MonsterInit();

	//사망 후 1초 뒤 시체 위치에 생성할 파티클. 액터와 따로 생성해서 몬스터가 사라져도 재생됨
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster|Death")
	TObjectPtr<UParticleSystem> DeathParticle;

	//공격 애니메이션이 있는 몬스터는 노티파이를 쓰고, 없는 몬스터는 기존 공격 타이머로 발사와 종료를 처리함
	bool bUseAttackMontage = true;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category= "Monster|Attack|Effect")
	TObjectPtr<UDecalComponent> AttackWarningEffect;

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
	const float MELEE_ATTACK_RADIUS_BASE = 200.0f;
	const float RANGED_ATTACK_RADIUS_BASE = 2000.0f;
	const float HYBRID_ATTACK_RADIUS_BASE = 1000.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	TObjectPtr<UShapeComponent> MonsterCollisionComponent;

	//머리 구를 붙일 뼈(소켓) 이름 스켈레톤마다 머리 뼈 이름이 달라서 값으로 뺌
	//이 뼈에 붙여야 고개를 숙이거나 쓰러지는 애니메이션에서도 머리 판정이 같이 움직임
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collision")
	FName HeadSocketName = TEXT("head");

	// 체력 컴포넌트에 저장될 수치, 얘는 읽기만 가능
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Monster|Stats|Health")
	float MaxHealth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Speed")
	float MinWalkSpeed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Speed")
	float MaxWalkSpeed;

	// 이 두 가지 범위중에서 랜덤하게 자동생성될 예정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|HealthRadius")
	float MinHealthRadius;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|HealthRadius")
	float MaxHealthRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|HealthRadius")
	float MonsterDamage;

	// 레그돌이 된 뒤 기상을 처음 시도하기까지의 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster|Ragdoll", meta = (ClampMin = "0.1"))
	float RagdollRecoverDelay = 3.0f;

	// 골반 속도가 이 값(cm/s) 이하가 되어야 멈춘 걸로 보고 일어남
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster|Ragdoll", meta = (ClampMin = "0"))
	float RagdollSettleSpeed = 50.0f;

	// 계속 굴러가도 이 시간(초)이 지나면 강제로 일어남
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster|Ragdoll", meta = (ClampMin = "0"))
	float RagdollMaxSettleWait = 3.0f;

	// 엎어진 상태에서 일어나는 몽타주. 비워두면 몽타주 없이 바로 복귀
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster|Ragdoll")
	TObjectPtr<UAnimMontage> GetUpFromFrontMontage;

	// 누운 상태에서 일어나는 몽타주. 비워두면 몽타주 없이 바로 복귀
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster|Ragdoll")
	TObjectPtr<UAnimMontage> GetUpFromBackMontage;

	// 쓰러진 위치와 자세를 판단할 골반 뼈 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster|Ragdoll")
	FName PelvisBoneName = TEXT("pelvis");

	// 엎어짐/누움 판정이 반대로 나오면 켜기 (스켈레톤마다 골반 축 방향이 다름)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster|Ragdoll")
	bool bFlipRagdollFaceUpCheck = false;

private:
	//피격과 사망에서 같은 물리 전환을 사용하되, 기상 예약은 살아 있을 때만 따로 처리함
	void ActivateRagdoll(const FVector& LaunchVelocity);
	//사망 연출 대기가 끝나면 파티클을 생성하고 몬스터를 제거함
	void FinishDeath();
	FTimerHandle DeathTimer;

	void PerformMeleeCheck();
	void SpawnAttackProjectile();

	void HandleAttackMontageEnded(
		UAnimMontage* Montage,
		bool bInterrupted
	);

	void FinishAttack(bool bSucceeded);

	bool bIsAttacking = false;
	bool bAttackExecuted = false;

	// 레그돌 상태 표시. 일어나는 몽타주가 끝날 때까지 true
	bool bIsRagdoll = false;

	// 기상 관련
	void TryEndRagdoll();
	void FinishGetUp();
	void HandleGetUpMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	FTimerHandle RagdollRecoverTimer;
	FTransform CachedMeshRelativeTransform;
	bool bIsGettingUp = false;
	float RagdollSettleWaitTime = 0.0f;
	const float RagdollSettleRetryInterval = 0.3f;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveGetUpMontage;

	TWeakObjectPtr<AActor> AttackTarget;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveAttackMontage;

	FTimerHandle AttackHitTimer;
	FTimerHandle AttackEndTimer;
};
