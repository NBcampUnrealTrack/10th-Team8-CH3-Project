// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AugmentTypes.h"
#include "ActiveSkillsComponent.generated.h"

class AActor;
class ACharacter;
class UAttackComponent;

//누구에게 얼마의 데미지를 줄지만 알리는 이벤트 실제 적용은 4계층이나 플레이어 쪽에서 함
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnActiveDamageRequested,
	const TArray<AActor*>&, Targets,
	float, Damage
);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DREAMVEIL_API UActiveSkillsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	//생성자
	UActiveSkillsComponent();

	//데미지 적용 요청 이벤트
	UPROPERTY(BlueprintAssignable, Category = "ActiveEvent")
	FOnActiveDamageRequested OnActiveDamageRequested;

	//번호로 액티브 증강 효과를 실행
	UFUNCTION(BlueprintCallable, Category = "Active")
	void ExecuteActiveAugment(EAugmentID AugmentID);

	//지정한 범위 안의 대상에게 줄 데미지를 계산하고 대상 목록을 반환
	UFUNCTION(BlueprintCallable, Category = "Active")
	void GatherDamageTargets(float Radius, float DamageRatio, TArray<AActor*>& OutTargets, float& OutDamage);

private:
	//공격력 참조 상위에서 만든 것을 찾아서 씀
	UPROPERTY(VisibleAnywhere, Category = "Active")
	TObjectPtr<UAttackComponent> AttackComponent;

	// 액티브 증강 보유 여부 중복 적용을 막기 위한 플래그
	//속도 저하
	bool bSlowEnemy;

	//범위 공격
	bool bAreaAttack;

	//지속 공격
	bool bContinuousAttack;

	//속도 저하 발동 타이머 핸들
	FTimerHandle SlowEnemyTimerHandle;

	//속도 저하 복구 타이머 핸들
	FTimerHandle SlowEnemyRestoreTimerHandle;

	//범위 공격 타이머 핸들
	FTimerHandle AreaAttackTimerHandle;

	//지속 공격 타이머 핸들
	FTimerHandle ContinuousAttackTimerHandle;

	//속도가 저하된 대상과 원래 이동 속도
	TMap<TWeakObjectPtr<ACharacter>, float> SlowedCharacters;

	//증강 번호와 효과 함수를 짝지은 테이블
	TMap<EAugmentID, TFunction<void()>> ActiveAugmentMap;

	//증강 효과 함수들을 테이블에 등록
	void RegisterActiveAugmentFunctions();

	//속도 저하 증강을 활성화하고 발동 타이머를 시작
	void ExecuteSlowEnemy();

	//범위 공격 증강을 활성화하고 발동 타이머를 시작
	void ExecuteAreaAttack();

	//지속 공격 증강을 활성화하고 발동 타이머를 시작
	void ExecuteContinuousAttack();

	//주변 대상의 이동 속도를 낮춤
	void ProcessSlowEnemyTick();

	//속도가 저하된 대상을 원래 속도로 되돌림
	void RestoreSlowedCharacters();

	//주변 대상에게 줄 범위 공격 데미지를 알림
	void ProcessAreaAttackTick();

	//주변 대상에게 줄 지속 공격 데미지를 알림
	void ProcessContinuousAttackTick();

	//지정한 범위 안의 대상을 찾음
	void FindTargetsInRadius(float Radius, TArray<AActor*>& OutTargets);

	//대상 목록과 데미지를 계산해 이벤트로 알림
	void RequestDamageInRadius(float Radius, float DamageRatio);

	//생명주기 함수
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
