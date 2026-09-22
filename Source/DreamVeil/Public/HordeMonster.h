// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MonsterBase.h"
#include "HordeMonster.generated.h"

class UCapsuleComponent;

UCLASS()
class DREAMVEIL_API AHordeMonster : public AMonsterBase
{
	GENERATED_BODY()
	
protected:
	TObjectPtr<UCapsuleComponent> MonsterCapsuleCollisionComponent;
	TObjectPtr<USkeletalMeshComponent> MonsterSkeletalMeshComponent;
public:
	AHordeMonster();
	virtual void BeginPlay() override;

	UFUNCTION()	//이벤트 등록할 함수에 이거 안하니까 경고문뜸...
	void MaxHealthChanged(float OldValue, float NewValue);

	virtual void MonsterInit() override;
};
