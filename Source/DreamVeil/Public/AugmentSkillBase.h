#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AugmentSkillBase.generated.h"

class AActor;
class UCombatStatsComponent;
class UDispatchTableComponent;
struct FHitResult;

//모든 증강 스킬의 공통 부모
//DispatchTableComponent가 증강을 처음 얻을 때 만들어서 들고 있음
//스킬마다 필요한 훅만 override하고 나머지는 비워둠
UCLASS(Abstract)
class DREAMVEIL_API UAugmentSkillBase : public UObject
{
	GENERATED_BODY()

public:
	//증강을 얻을 때마다 불림 반복 획득 증강은 여러 번 불림
	virtual void Apply();

	//주인 체력이 바뀐 뒤 불림
	virtual void OnHealthChanged(float OldValue, float NewValue);

	//주인이 데미지를 입힌 뒤 불림 FinalDamage는 상대 방어력을 뺀 값
	virtual void OnDamageDealt(float FinalDamage);

	//주인이 데미지를 받았을 때 공격자에게 돌려줄 반사 데미지 기본 0
	virtual float CalculateReflectDamage(float FinalDamage);

	//주인의 무기가 무언가를 맞혔을 때 불림 HitDamage는 방어력을 빼기 전 총 데미지
	virtual void OnWeaponHit(const FHitResult& HitResult, float HitDamage);

	//주인 컴포넌트가 사라질 때 타이머나 걸어둔 효과를 정리
	virtual void Deactivate();

	//타이머를 쓸 수 있도록 주인 컴포넌트의 월드를 돌려줌
	virtual UWorld* GetWorld() const override;

protected:
	//이 스킬을 들고 있는 컴포넌트
	UDispatchTableComponent* GetOwnerComponent() const;

	//주인 액터의 스탯 컴포넌트 스탯을 바꾸는 스킬이 씀 없으면 nullptr
	UCombatStatsComponent* GetStats() const;

	//컴포넌트가 붙어 있는 액터
	AActor* GetOwnerActor() const;
};
