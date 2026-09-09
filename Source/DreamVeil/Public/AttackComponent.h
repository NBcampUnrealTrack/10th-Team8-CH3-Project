// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttackComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DREAMVEIL_API UAttackComponent : public UActorComponent
{
	GENERATED_BODY()

private:

    // 공격력 수치
    float AttackPower;
    // 게임 시작 이벤트(공격력 Base로 초기화)
    virtual void BeginPlay() override;

public:

    // 공격력 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
    float BaseAttackPower;

    // 생성자
    UAttackComponent();

    // 공격력 증가
    void AddAttackPower(float Amount);

    // 공격력 배율 적용
    void MultiplyAttackPower(float Multiplier);

    // 공격력 Getter
    UFUNCTION(BlueprintCallable, Category = "Attack")
    float GetAttackPower();
};