#pragma once

#include "CoreMinimal.h"
#include "AugmentSkillBase.h"
#include "ActiveAugmentSkills.generated.h"

class ACharacter;

//액티브 증강 스킬 모음
//얻는 순간 타이머를 돌려서 주인 주변 대상에게 스스로 효과를 줌
//대상 탐색과 데미지 전송은 AugmentDamageLibrary로 함

//감속 주기마다 주변 캐릭터의 이동 속도를 잠시 낮춤
UCLASS()
class DREAMVEIL_API USlowEnemySkill : public UAugmentSkillBase
{
	GENERATED_BODY()

public:
	virtual void Apply() override;

	virtual void Deactivate() override;

private:
	//발동 타이머 핸들
	FTimerHandle SlowTimerHandle;

	//복구 타이머 핸들
	FTimerHandle RestoreTimerHandle;

	//속도가 저하된 대상과 원래 이동 속도
	TMap<TWeakObjectPtr<ACharacter>, float> SlowedCharacters;

	//주변 대상의 이동 속도를 낮춤
	void ProcessSlowTick();

	//속도가 저하된 대상을 원래 속도로 되돌림
	void RestoreSlowedCharacters();
};

//범위 공격 주기마다 주변 대상 전원에게 공격력 비율만큼 데미지
UCLASS()
class DREAMVEIL_API UAreaAttackSkill : public UAugmentSkillBase
{
	GENERATED_BODY()

public:
	virtual void Apply() override;

	virtual void Deactivate() override;

private:
	//발동 타이머 핸들
	FTimerHandle AreaAttackTimerHandle;

	//주변 대상에게 데미지
	void ProcessAreaAttackTick();
};

//지속 공격 짧은 주기마다 주변 대상 전원에게 공격력 비율만큼 데미지
UCLASS()
class DREAMVEIL_API UContinuousAttackSkill : public UAugmentSkillBase
{
	GENERATED_BODY()

public:
	virtual void Apply() override;

	virtual void Deactivate() override;

private:
	//발동 타이머 핸들
	FTimerHandle ContinuousAttackTimerHandle;

	//주변 대상에게 데미지
	void ProcessContinuousAttackTick();
};
