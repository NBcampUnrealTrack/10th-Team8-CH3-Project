// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AugmentTypes.h"
#include "SlowOpponentSkillComponent.generated.h"

class AActor;
class ACharacter;

//2계층
//상대 속도 저하 증강의 실제 효과를 구현하고 중복 적용을 막음
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DREAMVEIL_API USlowOpponentSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	//생성자
	USlowOpponentSkillComponent();

	//속도 저하 증강을 활성화하고 발동 타이머를 시작
	UFUNCTION(BlueprintCallable, Category = "Active")
	void ExecuteSlowOpponent();

	//증강을 보유 중인지 여부
	UFUNCTION(BlueprintCallable, Category = "Active")
	bool HasSlowOpponent();

private:
	//중복 적용을 막기 위한 플래그
	bool bSlowOpponent;

	//발동 타이머 핸들
	FTimerHandle SlowTimerHandle;

	//복구 타이머 핸들
	FTimerHandle RestoreTimerHandle;

	//속도가 저하된 대상과 원래 이동 속도
	TMap<TWeakObjectPtr<ACharacter>, float> SlowedCharacters;

	//주변 대상의 이동 속도를 낮춤
	void ProcessSlowTick();

	//속도가 저하된 대상을 원래 속도로 되돌림
	void RestoreSlowedCharacters();

	//생명주기 함수
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
