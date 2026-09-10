// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnMaxHealthChanged,
    float, OldValue,
    float, NewValue
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnCurrentHealthChanged,
    float, OldValue,
    float, NewValue
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDead);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DREAMVEIL_API UHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:

    //생성자
    UHealthComponent();

    //최대 체력 변화 이벤트
    UPROPERTY(BlueprintAssignable, Category = "HealthEvent")
    FOnMaxHealthChanged OnMaxHealthChanged;

    //현재 체력 변화 이벤트
    UPROPERTY(BlueprintAssignable, Category = "HealthEvent")
    FOnCurrentHealthChanged OnCurrentHealthChanged;

    //사망 이벤트
    UPROPERTY(BlueprintAssignable, Category = "HealthEvent")
    FOnDead OnDead;

    //죽음 상태 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    bool bIsDead;

    //최대 체력 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
    float MaxHealth;

    //데미지를 받아 현재 체력을 감소
    void ApplyDamage(float DamageAmount);

    //힐
    void HealHealth(float HealAmount);

    //현재 체력 설정
    void SetCurrentHealth(float NewCurrentHealth);

    //최대 체력 설정
    void SetMaxHealth(float NewMaxHealth);

    //현재 체력 Getter
    UFUNCTION(BlueprintCallable, Category = "Health")
    float GetCurrentHealth();

    //최대 체력 Getter
    UFUNCTION(BlueprintCallable, Category = "Health")
    float GetMaxHealth();

    //체력 퍼센티지 Getter
    UFUNCTION(BlueprintCallable, Category = "Health")
    float GetHealthPercentage();

private:
    //현재 체력 변수
    float CurrentHealth;

    // 생명주기 함수
    virtual void BeginPlay() override;
};