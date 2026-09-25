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

class UNiagaraSystem;
class UNiagaraComponent;

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

	// 증강 이펙트
	// 플레이어와 몬스터가 둘 다 가진 컴포넌트가 이것뿐이라 증강 연출을 여기 모음
	// 증강 스킬은 UObject라 월드에 이펙트를 직접 붙일 수 없어서 이 컴포넌트에게 부탁함

	//가시 갑옷 반사를 맞았을 때 몸에서 솟는 가시 비워두면 이펙트 없이 데미지만 들어감
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Effect")
	TObjectPtr<UNiagaraSystem> ThornReflectEffect;

	//불타는 동안 계속 재생할 불꽃 Loop Behavior를 Infinite로 만들어야 꺼질 때까지 남아 있음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Effect")
	TObjectPtr<UNiagaraSystem> OnFireEffect;

	//폭발탄이 터질 때 맞은 지점에 재생 Loop Behavior는 Once로 둘 것
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Effect")
	TObjectPtr<UNiagaraSystem> AreaAttackEffect;

	//가시 반사를 맞았다는 연출 반사 데미지가 들어올 때 AugmentDamageLibrary가 부름
	UFUNCTION(BlueprintCallable, Category = "Stats|Effect")
	void PlayThornReflectEffect();

	//폭발 연출을 지정한 위치에 재생 폭발탄 증강이 터진 자리에서 부름
	UFUNCTION(BlueprintCallable, Category = "Stats|Effect")
	void PlayAreaAttackEffect(const FVector& Location);

	//불타는 상태를 켜고 끔 켜면 불꽃이 몸에 붙고 끄면 사라짐
	//같은 상태로 두 번 불러도 이펙트가 겹치지 않음
	UFUNCTION(BlueprintCallable, Category = "Stats|Effect")
	void SetOnFire(bool bNewOnFire);

	//지금 불타고 있는지 애님 블루프린트나 UI가 읽을 수 있게 열어둠
	UFUNCTION(BlueprintPure, Category = "Stats|Effect")
	bool IsOnFire() const;

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
	//bIgnoreDefence가 true면 방어력을 빼지 않음 화염 데미지용
	UFUNCTION(BlueprintCallable, Category = "Combat")
	float ApplyIncomingDamage(float IncomingDamage, bool bIgnoreDefence = false);

protected:
	//생명주기 함수
	virtual void BeginPlay() override;

private:
	//불타는 중인지 SetOnFire가 켜고 끔
	bool bOnFire = false;

	//지금 몸에 붙어 있는 불꽃 컴포넌트 꺼질 때 없애려고 들고 있음 안 타면 nullptr
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> OnFireEffectComponent;

	//체력 공격력 방어력 기본값은 디테일 패널에서 캐릭터별로 입력
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (AllowPrivateAccess = "true"))
	FCombatStats Stats;
};
