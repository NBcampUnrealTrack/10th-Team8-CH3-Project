#pragma once

#include "CoreMinimal.h"
#include "AugmentSkillBase.h"
#include "ActiveAugmentSkills.generated.h"

class AActor;
class ACharacter;

//액티브 증강 스킬 모음
//셋 다 총이 AugmentDamageLibrary::ApplyWeaponHit로 적중을 알려야 발동함
//대상 탐색과 데미지 전송은 AugmentDamageLibrary로 함

//불이 붙은 대상 하나의 상태
struct FBurningTargetState
{
	//틱마다 줄 데미지
	float TickDamage = 0.0f;

	//남은 지속 시간
	float RemainingTime = 0.0f;

	//틱 타이머 핸들
	FTimerHandle TickTimerHandle;
};

//넉백 총에 맞은 적을 쏜 사람 반대쪽으로 살짝 밀어냄
//감속과 달리 상태를 들고 있지 않음 미는 순간 끝나서 되돌릴 것도 지속 시간도 없음
//래그돌이 아니라 이동 컴포넌트에 밀어넣는 이유 살아 있는 적을 넘어뜨리지 않고 자세만 흐트러뜨리려는 것
UCLASS()
class DREAMVEIL_API UKnockbackSkill : public UAugmentSkillBase
{
	GENERATED_BODY()

public:
	virtual void OnWeaponHit(const FHitResult& HitResult, float HitDamage) override;
};

//범위 공격 총알이 맞은 지점을 중심으로 반경 안의 대상에게 총 데미지 비율만큼 데미지
UCLASS()
class DREAMVEIL_API UAreaAttackSkill : public UAugmentSkillBase
{
	GENERATED_BODY()

public:
	virtual void OnWeaponHit(const FHitResult& HitResult, float HitDamage) override;
};

//지속 공격 총에 맞은 대상에게 일정 시간 동안 화염 데미지
UCLASS()
class DREAMVEIL_API UContinuousAttackSkill : public UAugmentSkillBase
{
	GENERATED_BODY()

public:
	virtual void OnWeaponHit(const FHitResult& HitResult, float HitDamage) override;

	virtual void Deactivate() override;

private:
	//불이 붙은 대상과 상태
	TMap<TWeakObjectPtr<AActor>, FBurningTargetState> BurningTargets;

	//한 대상에게 화염 데미지 한 번
	void ProcessBurnTick(TWeakObjectPtr<AActor> WeakTarget);
};
