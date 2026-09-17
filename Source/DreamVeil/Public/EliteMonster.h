// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MonsterBase.h"
#include "EliteMonster.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;

UCLASS()
class DREAMVEIL_API AEliteMonster : public AMonsterBase
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Health")
	float MaxHealth;

	TObjectPtr<UCapsuleComponent> MonsterCapsuleCollisionComponent;
	TObjectPtr<USkeletalMeshComponent> MonsterSkeletalMeshComponent;
public:
	AEliteMonster();

	virtual void BeginPlay() override;

	void MonsterInit();
};
