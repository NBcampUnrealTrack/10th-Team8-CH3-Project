#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatStats.h"
#include "CombatStatsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnCurrentHealthChanged,
	float, OldValue,
	float, NewValue
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnMaxHealthChanged,
	float, OldValue,
	float, NewValue
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDead);

//체력 공격력 방어력을 들고 바꾸고 알리는 컴포넌트
//증강은 전혀 모름 증강 없는 액터에도 이것만 붙여서 쓸 수 있음
//증강은 UDispatchTableComponent가 이 컴포넌트를 찾아서 스탯을 바꿈
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DREAMVEIL_API UCombatStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	//생성자
	UCombatStatsComponent();

	// 이벤트

	//현재 체력 변화 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Stats|Event")
	FOnCurrentHealthChanged OnCurrentHealthChanged;

	//최대 체력 변화 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Stats|Event")
	FOnMaxHealthChanged OnMaxHealthChanged;

	//사망 이벤트 체력이 0이 된 순간 한 번
	UPROPERTY(BlueprintAssignable, Category = "Stats|Event")
	FOnDead OnDead;

	// 스탯 조회

	//스탯 전체 복사본
	UFUNCTION(BlueprintPure, Category = "Stats")
	FCombatStats GetStats() const;

	//최종 공격력
	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetAttackPower() const;

	//최종 방어력
	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetDefencePower() const;

	//현재 체력
	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetCurrentHealth() const;

	//최대 체력
	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetMaxHealth() const;

	//체력 비율 0~1
	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetHealthPercentage() const;

	//죽었는지 여부
	UFUNCTION(BlueprintPure, Category = "Stats")
	bool IsDead() const;

	// 스탯 변경

	//공방체 기본값을 넣고 죽음 상태를 풀고 체력을 가득 채움 증강으로 더해진 값은 유지
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void InitStats(float NewMaxHealth, float NewDefencePower, float NewAttackPower);

	//추가 공격력을 더함
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void AddAttackPower(float Amount);

	//공격력 배율을 곱함
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void MultiplyAttackPower(float Multiplier);

	//추가 방어력을 더함
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void AddDefencePower(float Amount);

	//방어력 배율을 곱함
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void MultiplyDefencePower(float Multiplier);

	//최대 체력을 바꿈 현재 체력이 더 크면 최대 체력까지 줄임
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetMaxHealth(float NewMaxHealth);

	//현재 체력을 바꿈 0이면 사망 0보다 크면 살아 있는 상태로 되돌림
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetCurrentHealth(float NewCurrentHealth);

	//체력 회복 죽은 상태면 무시
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void Heal(float HealAmount);

	//죽음 상태를 풀고 체력을 가득 채움 부활이나 오브젝트 풀링 재사용용
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void ResetHealth();

	// 데미지 계산 AugmentDamageLibrary가 부름

	//이번 공격으로 줄 데미지
	UFUNCTION(BlueprintCallable, Category = "Combat")
	float CalculateOutgoingDamage() const;

	//받은 데미지를 방어력으로 줄여 체력에 적용하고 적용한 데미지를 반환 죽었거나 0 이하면 0
	UFUNCTION(BlueprintCallable, Category = "Combat")
	float ApplyIncomingDamage(float IncomingDamage);

protected:
	//생명주기 함수
	virtual void BeginPlay() override;

private:
	//체력 공격력 방어력 기본값은 디테일 패널에서 캐릭터별로 입력
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (AllowPrivateAccess = "true"))
	FCombatStats Stats;
};
