// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DefenceComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnDefencePowerChanged,
    float, OldValue,
    float, NewValue
);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DREAMVEIL_API UDefenceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
    //생성자
    UDefenceComponent();

    //방어력 변화 이벤트
    UPROPERTY(BlueprintAssignable, Category = "DefenceEvent")
    FOnDefencePowerChanged OnDefencePowerChanged;

    //기본 방어력
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defence")
    float BaseDefencePower;

    //방어력을 추가
    void AddDefencePower(float Amount);

    //방어력 배율 적용
    void MultiplyDefencePower(float Multiplier);

    UFUNCTION(BlueprintCallable, Category = "Defence")
    float GetDefencePower();

private:
    
    //추가 방어력
    float AdditionalDefencePower;

    //방어력 배율
    float DefenceMultiplier;

    //최종 방어력 
    float DefencePower;
    
    //최종 방어력을 계산
    void SetDefencePower();

    //생명주기 함수
    virtual void BeginPlay() override;


};