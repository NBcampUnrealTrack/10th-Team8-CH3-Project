#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AugmentTypes.h"
#include "AugmentPool.h"
#include "CombatStats.h"
#include "DispatchTableComponent.generated.h"

class UAugmentSkillBase;

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

//플레이어 보스 일반 몬스터에 붙는 유일한 증강 컴포넌트
//스탯 구조체 증강 풀 증강 번호별 스킬 테이블을 전부 들고 있음
//데미지 처리 순서는 AugmentDamageLibrary에 있고 여기는 계산과 적용만 제공
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DREAMVEIL_API UDispatchTableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	//생성자
	UDispatchTableComponent();

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

	// 데미지 AugmentDamageLibrary가 부름

	//이번 공격으로 줄 데미지
	UFUNCTION(BlueprintCallable, Category = "Combat")
	float CalculateOutgoingDamage() const;

	//받은 데미지를 방어력으로 줄여 체력에 적용하고 적용한 데미지를 반환 죽었거나 0 이하면 0
	UFUNCTION(BlueprintCallable, Category = "Combat")
	float ApplyIncomingDamage(float IncomingDamage);

	//받은 데미지로 공격자에게 돌려줄 반사 데미지 가시 갑옷이 없으면 0
	UFUNCTION(BlueprintCallable, Category = "Combat")
	float CalculateThornReflectDamage(float FinalDamage);

	//데미지를 입힌 뒤 보유 스킬 효과를 처리 흡혈
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ProcessOnDamageDealt(float FinalDamage);

	// 증강

	//뽑을 수 있는 증강 목록 에디터에서 바로 수정 가능 일반 몬스터는 안 씀
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
	FAugmentPool AugmentPool;

	//보상 UI 선택지 AUGMENT_CHOICE_COUNT개를 겹치지 않게 뽑음 풀은 건드리지 않음 플레이어용
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool DrawAugmentChoices(TArray<EAugmentID>& OutAugmentIDs);

	//증강을 적용하고 반복 획득이 안 되면 풀에서 제거 보상 UI에서 고른 것을 넘길 것
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool ApplyAugment(EAugmentID AugmentID);

	//풀에서 하나 뽑아서 바로 적용 보스용
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool DrawAndApplyAugment();

	//풀과 상관없이 증강 효과만 실행 일반 몬스터 타입별 패시브용 BeginPlay 이후에 부를 것
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool ExecuteAugment(EAugmentID AugmentID);

	//정해진 증강 묶음을 한 번에 실행
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void ExecuteAugments(const TArray<EAugmentID>& AugmentIDs);

	//같은 증강을 여러 번 실행 엔드리스 웨이브 체공방 증가용
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void ExecuteAugmentRepeat(EAugmentID AugmentID, int32 RepeatCount);

	//이미 얻은 증강인지 여부
	UFUNCTION(BlueprintPure, Category = "Augment")
	bool HasAcquiredAugment(EAugmentID AugmentID) const;

protected:
	//생명주기 함수
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	//체력 공격력 방어력 기본값은 디테일 패널에서 캐릭터별로 입력
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats", meta = (AllowPrivateAccess = "true"))
	FCombatStats Stats;

	//디스패치 테이블 증강 번호마다 어떤 스킬 클래스를 만들지
	UPROPERTY(VisibleAnywhere, Category = "Augment")
	TMap<EAugmentID, TSubclassOf<UAugmentSkillBase>> SkillClassTable;

	//지금까지 얻은 증강의 스킬 객체 처음 얻을 때 만들어짐
	UPROPERTY(Transient)
	TMap<EAugmentID, TObjectPtr<UAugmentSkillBase>> AcquiredSkills;

	//증강 번호와 스킬 클래스를 테이블에 등록
	void RegisterSkillClasses();

	//얻은 스킬이 있으면 돌려주고 없으면 테이블을 보고 새로 만듦 테이블에 없는 번호면 nullptr
	UAugmentSkillBase* FindOrCreateSkill(EAugmentID AugmentID);
};
