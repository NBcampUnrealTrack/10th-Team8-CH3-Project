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
public:
	AEliteMonster();

	virtual void BeginPlay() override;

	void MonsterInit() override;

	UFUNCTION(BlueprintPure)
	EEliteMonsterType GetEliteType() const;

protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|EliteType")
	EEliteMonsterType EliteType;

	TObjectPtr<UCapsuleComponent> MonsterCapsuleCollisionComponent;
	TObjectPtr<USkeletalMeshComponent> MonsterSkeletalMeshComponent;

};
