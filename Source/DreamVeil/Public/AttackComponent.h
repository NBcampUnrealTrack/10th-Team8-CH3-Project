// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttackComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnAttackPowerChanged,
    float, OldValue,
    float, NewValue
);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DREAMVEIL_API UAttackComponent : public UActorComponent
{
	GENERATED_BODY()

public:

    //생성자
    UAttackComponent();

    //공격력 변화 이벤트
    UPROPERTY(BlueprintAssignable, Category = "AttackEvent")
    FOnAttackPowerChanged OnAttackPowerChanged;

    //기본 공격력
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
    float BaseAttackPower;

    //공격력을 추가
    void AddAttackPower(float Amount);

    //공격력 배율 적용
    void MultiplyAttackPower(float Multiplier);

    //최종 공격력을 계산 BeginPlay 이후 BaseAttackPower를 바꿨으면 불러줄 것
    void SetAttackPower();

    UFUNCTION(BlueprintCallable, Category = "Attack")
    float GetAttackPower();
private:

    //추가 공격력
    float AdditionalAttackPower;

    //공격력 배율
    float AttackMultiplier;

    //최종 공격력
    float AttackPower;

    //생명주기 함수
    virtual void BeginPlay() override;


};