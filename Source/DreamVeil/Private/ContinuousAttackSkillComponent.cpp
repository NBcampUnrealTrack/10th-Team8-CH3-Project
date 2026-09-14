// Fill out your copyright notice in the Description page of Project Settings.


#include "ContinuousAttackSkillComponent.h"

#include "AugmentDamageLibrary.h"
#include "Engine/World.h"
#include "TimerManager.h"

//생성자
UContinuousAttackSkillComponent::UContinuousAttackSkillComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    bContinuousAttack = false;
}

//지속 공격 증강을 활성화하고 발동 타이머를 시작
void UContinuousAttackSkillComponent::ExecuteContinuousAttack()
{
    if (bContinuousAttack)
    {
        return;
    }

    bContinuousAttack = true;

    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        ContinuousAttackTimerHandle,
        this,
        &UContinuousAttackSkillComponent::ProcessContinuousAttackTick,
        CONTINUOUS_ATTACK_INTERVAL,
        true
    );
}

//증강을 보유 중인지 여부
bool UContinuousAttackSkillComponent::HasContinuousAttack()
{
    return bContinuousAttack;
}

//범위 안의 대상 목록과 줄 데미지를 계산 실제 적용은 하지 않음
void UContinuousAttackSkillComponent::GatherDamageTargets(TArray<AActor*>& OutTargets, float& OutDamage)
{
    AActor* OwnerActor = GetOwner();

    UAugmentDamageLibrary::FindTargetsInRadius(OwnerActor, CONTINUOUS_ATTACK_RADIUS, OutTargets);

    OutDamage = UAugmentDamageLibrary::GetOutgoingDamage(OwnerActor) * CONTINUOUS_ATTACK_DAMAGE_RATIO;
}

//주변 대상에게 지속 공격 데미지를 적용
void UContinuousAttackSkillComponent::ProcessContinuousAttackTick()
{
    TArray<AActor*> FoundTargets;
    float Damage = 0.0f;

    GatherDamageTargets(FoundTargets, Damage);

    if (FoundTargets.Num() == 0)
    {
        return;
    }

    if (Damage <= 0.0f)
    {
        return;
    }

    UAugmentDamageLibrary::ApplyAugmentDamage(GetOwner(), FoundTargets, Damage);
}

//생명주기 함수
void UContinuousAttackSkillComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UWorld* World = GetWorld();

    if (World)
    {
        World->GetTimerManager().ClearTimer(ContinuousAttackTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}
