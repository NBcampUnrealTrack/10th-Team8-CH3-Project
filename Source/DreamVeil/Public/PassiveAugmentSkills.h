#pragma once

#include "CoreMinimal.h"
#include "AugmentSkillBase.h"
#include "PassiveAugmentSkills.generated.h"

//패시브 증강 스킬 모음
//스탯을 직접 바꾸거나 체력 변화 흡혈 반사 같은 훅에 반응함

//공격력 증가 얻을 때마다 추가 공격력이 더해짐
UCLASS()
class DREAMVEIL_API UAttackUpSkill : public UAugmentSkillBase
{
	GENERATED_BODY()

public:
	virtual void Apply() override;
};

//방어력 증가 얻을 때마다 추가 방어력이 더해짐
UCLASS()
class DREAMVEIL_API UDefenceUpSkill : public UAugmentSkillBase
{
	GENERATED_BODY()

public:
	virtual void Apply() override;
};

//체력 증가 얻을 때마다 최대 체력이 늘고 늘어난 만큼 회복
UCLASS()
class DREAMVEIL_API UHealthUpSkill : public UAugmentSkillBase
{
	GENERATED_BODY()

public:
	virtual void Apply() override;
};

//광전사 체력이 일정 비율 이하면 공격력 배율 적용
UCLASS()
class DREAMVEIL_API UBerserkerSkill : public UAugmentSkillBase
{
	GENERATED_BODY()

public:
	virtual void Apply() override;

	virtual void OnHealthChanged(float OldValue, float NewValue) override;

private:
	//배율이 현재 적용된 상태인지 중복 곱하기 방지
	bool bActivated = false;

	//체력 비율을 보고 배율을 켜거나 끔
	void UpdateState();
};

//최후의 요새 체력이 일정 비율 이하면 방어력 배율 적용
UCLASS()
class DREAMVEIL_API ULastFortressSkill : public UAugmentSkillBase
{
	GENERATED_BODY()

public:
	virtual void Apply() override;

	virtual void OnHealthChanged(float OldValue, float NewValue) override;

private:
	//배율이 현재 적용된 상태인지 중복 곱하기 방지
	bool bActivated = false;

	//체력 비율을 보고 배율을 켜거나 끔
	void UpdateState();
};

//가시 갑옷 받은 데미지의 일정 비율을 공격자에게 반사
UCLASS()
class DREAMVEIL_API UThornArmorSkill : public UAugmentSkillBase
{
	GENERATED_BODY()

public:
	virtual float CalculateReflectDamage(float FinalDamage) override;
};

//흡혈 입힌 데미지의 일정 비율만큼 회복
UCLASS()
class DREAMVEIL_API UVampireSkill : public UAugmentSkillBase
{
	GENERATED_BODY()

public:
	virtual void OnDamageDealt(float FinalDamage) override;
};

//재생력 일정 간격마다 체력 회복
UCLASS()
class DREAMVEIL_API URegenerationSkill : public UAugmentSkillBase
{
	GENERATED_BODY()

public:
	virtual void Apply() override;

	virtual void Deactivate() override;

private:
	//회복 타이머 핸들
	FTimerHandle RegenerationTimerHandle;

	//일정 간격마다 체력을 회복
	void ProcessRegenerationTick();
};
