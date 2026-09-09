#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PassiveSkillsComponent.generated.h"

class UAttackComponent;
class UDefenceComponent;
class UHealthComponent;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class DREAMVEIL_API UPassiveSkillsComponent : public UActorComponent
{
	GENERATED_BODY()

private:
	//공방체 매크로 다른 곳에서는 건들면 안 되지만 작동하는지 봐야하기 때문에 visible으로 처리함
	//공격
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Categroy="Passive")
	TObjectPtr<UAttackComponent> AttackComponent;
	//수비
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Categroy = "Passive")
	TObjectPtr<UDefenceComponent> DefenceComponent;
	//체력
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Categroy = "Passive")
	TObjectPtr<UHealthComponent> HealthComponent;
	//공방체 컴포넌트들 초기화
	void InitializeBasicStatsComponents();
public:
	//생성자
	UPassiveSkillsComponent();
	// 패시브 증강
	//광전사 체력 30퍼 이하일시 공격력 상승
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Passive")
	bool bBerserker;
	//흡혈
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Passive")
	bool bVampire;
	//방어력 증가
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Passive")
	bool bDefencePowerUp;
	//가시 갑옷
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Passive")
	bool bThornArmor;
	//재생력
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Passive")
	bool bRegeneration;
	//공격력 증가
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Passive")
	bool bAttackPowerUp;
};
