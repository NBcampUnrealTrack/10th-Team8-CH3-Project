#pragma once

#include "CoreMinimal.h"
#include "CombatStats.generated.h"

//체력 공격력 방어력 숫자 묶음
//여기는 계산만 하고 값 변경과 이벤트 방송은 UCombatStatsComponent가 함
USTRUCT(BlueprintType)
struct FCombatStats
{
	GENERATED_BODY()

	//최대 체력
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Health")
	float MaxHealth = 100.0f;

	//현재 체력 BeginPlay에서 최대 체력으로 채움
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats|Health")
	float CurrentHealth = 100.0f;

	//죽음 상태
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats|Health")
	bool bIsDead = false;

	//기본 공격력
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Attack")
	float BaseAttackPower = 0.0f;

	//증강으로 더해진 공격력
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats|Attack")
	float AdditionalAttackPower = 0.0f;

	//공격력 배율 곱해서 누적 해제할 때는 역수를 곱함
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats|Attack")
	float AttackMultiplier = 1.0f;

	//기본 방어력
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Defence")
	float BaseDefencePower = 0.0f;

	//증강으로 더해진 방어력
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats|Defence")
	float AdditionalDefencePower = 0.0f;

	//방어력 배율 곱해서 누적 해제할 때는 역수를 곱함
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats|Defence")
	float DefenceMultiplier = 1.0f;

	//최종 공격력 매번 계산하므로 기본값을 바꾸면 바로 반영됨 바닥값 1
	float GetAttackPower() const
	{
		return FMath::Max((BaseAttackPower + AdditionalAttackPower) * AttackMultiplier, 1.0f);
	}

	//최종 방어력 음수만 막음
	float GetDefencePower() const
	{
		return FMath::Max((BaseDefencePower + AdditionalDefencePower) * DefenceMultiplier, 0.0f);
	}

	//체력 비율 0~1
	float GetHealthPercentage() const
	{
		if (MaxHealth <= 0.0f)
		{
			return 0.0f;
		}

		return CurrentHealth / MaxHealth;
	}
};
