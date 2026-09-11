#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AugmentTypes.h"
#include "PassiveSkillsComponent.generated.h"

class AActor;
class AController;
class UDamageType;
class UAttackComponent;
class UDefenceComponent;
class UHealthComponent;

//2계층
//패시브 증강의 실제 효과를 구현하고 중복 적용을 막음
//받는 데미지 계산은 소유 액터의 OnTakeAnyDamage에 물려서 자동으로 돎
//따라서 5계층은 TakeDamage를 직접 건드릴 필요가 없음
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DREAMVEIL_API UPassiveSkillsComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	//생성자
	UPassiveSkillsComponent();

	//이번 공격으로 줄 데미지를 계산 평타 데미지를 만들 때 씀
	UFUNCTION(BlueprintCallable, Category = "Combat")
	float CalculateOutgoingDamage();

	//데미지를 입힌 뒤 보유 중인 패시브 효과를 처리 흡혈
	//때린 쪽은 실제로 얼마가 깎였는지 모르기 때문에 맞은 쪽이 대신 불러줌
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ProcessOnDamageDealt(float DamageAmount);

	// 1계층 Getter
	// 5계층에서 체력바나 공격력 표시를 붙일 때 여기서 가져다 쓸 것
	// 액터에 HealthComponent를 따로 또 만들면 체력이 두 개가 되어 갈라짐

	//공격 컴포넌트 Getter
	UFUNCTION(BlueprintCallable, Category = "Passive")
	UAttackComponent* GetAttackComponent();

	//수비 컴포넌트 Getter
	UFUNCTION(BlueprintCallable, Category = "Passive")
	UDefenceComponent* GetDefenceComponent();

	//체력 컴포넌트 Getter
	UFUNCTION(BlueprintCallable, Category = "Passive")
	UHealthComponent* GetHealthComponent();

	// 증강 효과 함수들
	// 3계층 디스패치 테이블이 직접 부르기 때문에 public

	//공격력 증가 증강을 적용
	UFUNCTION(BlueprintCallable, Category = "Passive")
	void ExecuteAttackPowerUp();

	//방어력 증가 증강을 적용
	UFUNCTION(BlueprintCallable, Category = "Passive")
	void ExecuteDefencePowerUp();

	//체력 증가 증강을 적용
	UFUNCTION(BlueprintCallable, Category = "Passive")
	void ExecuteHealthUp();

	//광전사 증강을 활성화하고 즉시 상태를 반영
	UFUNCTION(BlueprintCallable, Category = "Passive")
	void ExecuteBerserker();

	//가시 갑옷 증강을 활성화
	UFUNCTION(BlueprintCallable, Category = "Passive")
	void ExecuteThornArmor();

	//흡혈 증강을 활성화
	UFUNCTION(BlueprintCallable, Category = "Passive")
	void ExecuteVampire();

	//재생력 증강을 활성화하고 회복 타이머를 시작
	UFUNCTION(BlueprintCallable, Category = "Passive")
	void ExecuteRegeneration();

private:
	//1계층 공방체
	//공격
	UPROPERTY(VisibleAnywhere, Category = "Passive")
	TObjectPtr<UAttackComponent> AttackComponent;
	//수비
	UPROPERTY(VisibleAnywhere, Category = "Passive")
	TObjectPtr<UDefenceComponent> DefenceComponent;
	//체력
	UPROPERTY(VisibleAnywhere, Category = "Passive")
	TObjectPtr<UHealthComponent> HealthComponent;

	// 패시브 증강 보유 여부 중복 적용을 막기 위한 플래그
	//광전사
	bool bBerserker;

	//흡혈
	bool bVampire;

	//가시 갑옷
	bool bThornArmor;

	//재생력
	bool bRegeneration;

	//광전사 배율이 현재 적용된 상태인지 여부
	bool bBerserkerActivated;

	//재생력 타이머 핸들
	FTimerHandle RegenerationTimerHandle;

	//언리얼 데미지 시스템이 올려주는 데미지를 받아 방어력 체력 가시갑옷 흡혈을 처리
	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	//체력 상태에 따라 광전사 효과를 갱신
	void UpdateBerserkerState();

	//현재 체력이 바뀔 때 광전사 상태를 갱신
	UFUNCTION()
	void HandleCurrentHealthChanged(float OldValue, float NewValue);

	//일정 간격마다 체력을 회복
	void ProcessRegenerationTick();

	//생명주기 함수
	virtual void BeginPlay() override;
};
