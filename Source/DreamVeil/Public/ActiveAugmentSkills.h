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
//감속 상태는 모든 감속 스킬이 같이 씀 공격자가 여럿이어도 원래 속도는 한 번만 저장하고 겹쳐서 느려지지 않음
//되돌리는 타이머는 스킬 객체와 상관없이 돌아서 쏜 사람이 먼저 사라져도 대상은 제때 원래 속도로 돌아옴
UCLASS()
class DREAMVEIL_API USlowEnemySkill : public UAugmentSkillBase
{
	GENERATED_BODY()

public:
	virtual void OnWeaponHit(const FHitResult& HitResult, float HitDamage) override;

private:
	//감속 중인 대상과 상태 모든 감속 스킬 객체가 공유
	static TMap<TWeakObjectPtr<ACharacter>, FSlowedCharacterState> SlowedCharacters;

	//한 대상의 이동 속도를 원래대로 되돌림
	static void RestoreCharacter(TWeakObjectPtr<ACharacter> WeakTarget);

	//이미 사라진 대상의 상태를 목록에서 지움 레벨이 바뀌어 타이머가 사라진 경우 대비
	static void RemoveInvalidTargets();
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
