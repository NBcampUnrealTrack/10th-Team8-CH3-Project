// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MonsterType.h"
#include "MonsterBase.generated.h"

class UShapeComponent;
class UCombatStatsComponent;
class UDispatchTableComponent;

UCLASS()
class DREAMVEIL_API AMonsterBase : public ACharacter
{
	GENERATED_BODY()
public:
	AMonsterBase();

	float GetMonsterAttackRange() const;

	//받은 데미지를 증강 라이브러리로 넘김 이게 없으면 체력이 안 깎임
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	//스탯 컴포넌트 체력 공격력 방어력과 사망 이벤트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	TObjectPtr<UCombatStatsComponent> MonsterCombatStats;

	//증강 컴포넌트 타입별 패시브와 웨이브 강화
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Augment")
	TObjectPtr<UDispatchTableComponent> MonsterDispatchTable;

	// 이게 실제 메쉬 컴포넌트 이야기. skeletal 버리고 쓸거 상정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TObjectPtr<UMeshComponent> MonsterMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MonsterWalkSpeed;

	// 몬스터가 원거리인지 근거리인지 체크
	UFUNCTION(BlueprintPure, Category = "Monster|Attack")
	EMonsterAttackType GetAttackType() const;

	// GetMesh대신 이거 써주세요.
	UFUNCTION(BlueprintPure, Category= "Monster|Mesh")
	UMeshComponent* GetMonsterMesh() const;
protected:
	virtual void BeginPlay() override;
	virtual void MonsterInit();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Monster|Attack")
	EMonsterAttackType AttackType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	float MonsterAttackRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
	TObjectPtr<UShapeComponent> MonsterCollisionComponent;

	// 체력 컴포넌트에 저장될 수치, 얘는 읽기만 가능
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats|Health")
	float MaxHealth;

	// 이 두 가지 범위중에서 랜덤하게 자동생성될 예정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|HealthRadius")
	float MinHealthRadius;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|HealthRadius")
	float MaxHealthRadius;
};
