// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AugmentTypes.h"
#include "ContinuousAttackSkillComponent.generated.h"

class AActor;

//2계층
//지속 공격 증강의 실제 효과를 구현하고 중복 적용을 막음
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DREAMVEIL_API UContinuousAttackSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	//생성자
	UContinuousAttackSkillComponent();

	//지속 공격 증강을 활성화하고 발동 타이머를 시작
	UFUNCTION(BlueprintCallable, Category = "Active")
	void ExecuteContinuousAttack();

	//증강을 보유 중인지 여부
	UFUNCTION(BlueprintCallable, Category = "Active")
	bool HasContinuousAttack();

	//범위 안의 대상 목록과 줄 데미지를 계산 실제 적용은 하지 않음
	UFUNCTION(BlueprintCallable, Category = "Active")
	void GatherDamageTargets(TArray<AActor*>& OutTargets, float& OutDamage);

private:
	//중복 적용을 막기 위한 플래그
	bool bContinuousAttack;

	//발동 타이머 핸들
	FTimerHandle ContinuousAttackTimerHandle;

	//주변 대상에게 지속 공격 데미지를 적용
	void ProcessContinuousAttackTick();

	//생명주기 함수
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
