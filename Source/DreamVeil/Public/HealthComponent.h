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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
    int32 CurrentHealth;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
    int32 MaxHealth;

public:

    UHealthComponent();

    // 대상에게 데미지를 적용한다.
    virtual void ApplyDamage(int32 DamageAmount);
};

