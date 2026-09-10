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
	// 패시브 증강
	//광전사
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Passive")
	bool bBerserker;

	//흡혈
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Passive")
	bool bVampire;

	//가시 갑옷
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Passive")
	bool bThornArmor;

	//재생력
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Passive")
	bool bRegeneration;

	//번호로 패시브 증강 효과를 실행
	UFUNCTION(BlueprintCallable, Category = "Passive")
	void ExecutePassiveAugment(EAugmentID AugmentID);

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

	//재생력 타이머 핸들
	FTimerHandle RegenerationTimerHandle;

	//증강 번호와 효과 함수를 짝지은 테이블
	TMap<EAugmentID, TFunction<void()>> PassiveAugmentMap;

	//증강 효과 함수들을 테이블에 등록
	void RegisterPassiveAugmentFunctions();

	//공격력 증가 증강 효과
	void ExecuteAttackPowerUp();

	//방어력 증가 증강 효과
	void ExecuteDefencePowerUp();

	//체력 증가 증강 효과
	void ExecuteHealthUp();

	//광전사 증강 획득
	void ExecuteBerserker();

	//가시 갑옷 증강 획득
	void ExecuteThornArmor();

	//흡혈 증강 획득
	void ExecuteVampire();

	//재생력 증강 획득
	void ExecuteRegeneration();

	//일정 간격마다 체력을 회복
	void ProcessRegenerationTick();

	//생명주기 함수
	virtual void BeginPlay() override;
};