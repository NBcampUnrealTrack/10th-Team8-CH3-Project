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

    // 방어력 수치
    float DefencePower;

    // 게임 시작 이벤트(방어력 Base로 초기화)
    virtual void BeginPlay() override;

public:

    // 방어력 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defence")
    float BaseDefencePower;

    // 생성자
    UDefenceComponent();

    // 방어력 증가
    void AddDefencePower(float Amount);

    // 방어력 배율 적용
    void MultiplyDefencePower(float Multiplier);

    // 방어력 Getter
    UFUNCTION(BlueprintCallable, Category = "Defence")
    float GetDefencePower();
};