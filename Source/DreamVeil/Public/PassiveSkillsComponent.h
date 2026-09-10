#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AugmentTypes.h"
#include "PassiveSkillsComponent.generated.h"

class UAttackComponent;
class UDefenceComponent;
class UHealthComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DREAMVEIL_API UPassiveSkillsComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	//생성자
	UPassiveSkillsComponent();

	//번호로 패시브 증강 효과를 실행
	UFUNCTION(BlueprintCallable, Category = "Passive")
	void ExecutePassiveAugment(EAugmentID AugmentID);

	//이번 공격으로 줄 데미지를 계산
	UFUNCTION(BlueprintCallable, Category = "Combat")
	float CalculateOutgoingDamage();

	//받은 데미지를 방어력으로 줄여 체력에 적용하고 반사 데미지를 반환
	UFUNCTION(BlueprintCallable, Category = "Combat")
	float ApplyIncomingDamage(float IncomingDamage);

	//데미지를 입힌 뒤 보유 중인 패시브 효과를 처리
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ProcessOnDamageDealt(float DamageAmount);

private:
	//공방체 매크로 다른 곳에서는 건들면 안 되지만 작동하는지 봐야하기 때문에 visible으로 처리함
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

	//증강 번호와 효과 함수를 짝지은 테이블
	TMap<EAugmentID, TFunction<void()>> PassiveAugmentMap;

	//증강 효과 함수들을 테이블에 등록
	void RegisterPassiveAugmentFunctions();

	//공격력 증가 증강을 적용
	void ExecuteAttackPowerUp();

	//방어력 증가 증강을 적용
	void ExecuteDefencePowerUp();

	//체력 증가 증강을 적용
	void ExecuteHealthUp();

	//광전사 증강을 활성화하고 즉시 상태를 반영
	void ExecuteBerserker();

	//가시 갑옷 증강을 활성화
	void ExecuteThornArmor();

	//흡혈 증강을 활성화
	void ExecuteVampire();

	//재생력 증강을 활성화하고 회복 타이머를 시작
	void ExecuteRegeneration();

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