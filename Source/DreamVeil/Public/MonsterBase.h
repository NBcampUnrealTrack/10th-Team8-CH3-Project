// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MonsterBase.generated.h"

class UShapeComponent;
class UPassiveSkillsComponent;

UCLASS()
class DREAMVEIL_API AMonsterBase : public ACharacter
{
	GENERATED_BODY()
private:

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	float MonsterAttackRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	TObjectPtr<UShapeComponent> MonsterCollisionComponent;
public:	
	AMonsterBase();
	
	float GetMonsterAttackRange() const;

	TObjectPtr<UPassiveSkillsComponent> MonsterPassive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TObjectPtr<UMeshComponent> MonsterMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MonsterWalkSpeed;
};
