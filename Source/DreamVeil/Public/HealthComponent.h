// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DREAMVEIL_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

private:
    //현재 체력 변수
    float CurrentHealth;

    // 생명주기 함수
    virtual void BeginPlay() override;
public:
    //죽음 상태 변수
  UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    bool bIsDead;
    //최대 체력 변수
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
    float MaxHealth;
    //생성자
    UHealthComponent();
    //힐
    void HealHealth(float HealAmount);
    //현재 체력 설정
    void SetCurrentHealth(float CurrentHealth);
    //최대 체력 설정
    void SetMaxHealth(float MaxHealth);
    //현재 체력 Getter
    UFUNCTION(BlueprintCallable, Category = "Health")
    float GetCurrentHealth();
    //최대 체력 Getter
    UFUNCTION(BlueprintCallable, Category = "Health")
    float GetMaxHealth();
    //체력 퍼센티지 Getter
    UFUNCTION(BlueprintCallable, Category = "Health")
    float GetHealthPercentage();
   
};

