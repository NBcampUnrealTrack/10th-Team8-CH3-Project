// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AugmentTypes.h"
#include "DispatchTableForPlayerAndBossComponent.generated.h"

class UPassiveSkillsComponent;
class USlowOpponentSkillComponent;
class UWideRangeAttackSkillComponent;
class UContinuousAttackSkillComponent;

//3계층
//증강 번호를 받아 어느 2계층 컴포넌트의 어느 함수를 부를지 고르는 디스패치 테이블
//플레이어와 보스는 패시브 액티브를 전부 쓰기 때문에 통합 테이블 하나를 씀
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DREAMVEIL_API UDispatchTableForPlayerAndBossComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	//생성자
	UDispatchTableForPlayerAndBossComponent();

	//번호로 증강 효과를 실행
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void ExecuteAugment(EAugmentID AugmentID);

	//테이블에 등록된 증강인지 확인
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool HasAugment(EAugmentID AugmentID);

	//패시브 스킬 Getter
	UFUNCTION(BlueprintCallable, Category = "Augment")
	UPassiveSkillsComponent* GetPassiveSkillsComponent();

private:
	//2계층 패시브 스킬
	UPROPERTY(VisibleAnywhere, Category = "Augment")
	TObjectPtr<UPassiveSkillsComponent> PassiveSkillsComponent;

	//2계층 상대 속도 저하
	UPROPERTY(VisibleAnywhere, Category = "Augment")
	TObjectPtr<USlowOpponentSkillComponent> SlowOpponentSkillComponent;

	//2계층 범위 공격
	UPROPERTY(VisibleAnywhere, Category = "Augment")
	TObjectPtr<UWideRangeAttackSkillComponent> WideRangeAttackSkillComponent;

	//2계층 지속 공격
	UPROPERTY(VisibleAnywhere, Category = "Augment")
	TObjectPtr<UContinuousAttackSkillComponent> ContinuousAttackSkillComponent;

	//증강 번호와 효과 함수를 짝지은 통합 테이블
	TMap<EAugmentID, TFunction<void()>> AugmentMap;

	//증강 효과 함수들을 테이블에 등록
	void RegisterAugmentFunctions();
};
