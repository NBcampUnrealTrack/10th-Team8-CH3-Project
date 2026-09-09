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

    //추가 공격력
    float AdditionalAttackPower;

    //공격력 배율
    float AttackMultiplier;

    //최종 공격력
    float AttackPower;

    //최종 공격력을 계산
    void SetAttackPower();

    //생명주기 함수
    virtual void BeginPlay() override;

public:

    //생성자
    UAttackComponent();
   
    //기본 공격력
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
    float BaseAttackPower;

    //공격력을 추가
    void AddAttackPower(float Amount);

    //공격력 배율 적용
    void MultiplyAttackPower(float Multiplier);

    UFUNCTION(BlueprintCallable, Category = "Attack")
    float GetAttackPower();
};