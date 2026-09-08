// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DefenceComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DREAMVEIL_API UDefenceComponent : public UActorComponent
{
	GENERATED_BODY()

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

public:

    //기본 방어력
    UPROPERTY(EditAnywhere, BlueprintReadWrite,Category="Defence")
    float BaseDefencePower;

    //생성자
    UDefenceComponent();

    //방어력을 추가
    void AddDefencePower(float Amount);

    //방어력 배율 적용
    void MultiplyDefencePower(float Multiplier); 

    UFUNCTION(BlueprintCallable, Category="Defence")
    float GetDefencePower();
};