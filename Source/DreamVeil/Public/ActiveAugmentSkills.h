#pragma once

#include "CoreMinimal.h"
#include "AugmentSkillBase.h"
#include "ActiveAugmentSkills.generated.h"

class AActor;
class ACharacter;

//액티브 증강 스킬 모음
//셋 다 총이 AugmentDamageLibrary::ApplyWeaponHit로 적중을 알려야 발동함
//대상 탐색과 데미지 전송은 AugmentDamageLibrary로 함

//감속 걸린 대상 하나의 상태
struct FSlowedCharacterState
{
	//감속 전 원래 이동 속도
	float OriginalWalkSpeed = 0.0f;

	//감속을 푸는 타이머 핸들
	FTimerHandle RestoreTimerHandle;
};

//독에 걸린 대상 하나의 상태
struct FPoisonedTargetState
{
	//틱마다 줄 데미지
	float TickDamage = 0.0f;

	//남은 지속 시간
	float RemainingTime = 0.0f;

	//틱 타이머 핸들
	FTimerHandle TickTimerHandle;
};

//감속 총에 맞은 캐릭터의 이동 속도를 일정 시간 낮춤
UCLASS()
class DREAMVEIL_API USlowEnemySkill : public UAugmentSkillBase
{
	GENERATED_BODY()

public:
	virtual void OnWeaponHit(const FHitResult& HitResult, float HitDamage) override;

	virtual void Deactivate() override;

private:
	//감속 중인 대상과 상태
	TMap<TWeakObjectPtr<ACharacter>, FSlowedCharacterState> SlowedCharacters;

	//한 대상의 이동 속도를 원래대로 되돌림
	void RestoreCharacter(TWeakObjectPtr<ACharacter> WeakTarget);
};

//범위 공격 총알이 맞은 지점을 중심으로 반경 안의 대상에게 총 데미지 비율만큼 데미지
UCLASS()
class DREAMVEIL_API UAreaAttackSkill : public UAugmentSkillBase
{
	GENERATED_BODY()

public:
	virtual void OnWeaponHit(const FHitResult& HitResult, float HitDamage) override;
};

//지속 공격 총에 맞은 대상에게 일정 시간 동안 독 데미지
UCLASS()
class DREAMVEIL_API UContinuousAttackSkill : public UAugmentSkillBase
{
	GENERATED_BODY()

public:
	virtual void OnWeaponHit(const FHitResult& HitResult, float HitDamage) override;

	virtual void Deactivate() override;

private:
	//독에 걸린 대상과 상태
	TMap<TWeakObjectPtr<AActor>, FPoisonedTargetState> PoisonedTargets;

	//한 대상에게 독 데미지 한 번
	void ProcessPoisonTick(TWeakObjectPtr<AActor> WeakTarget);
};
