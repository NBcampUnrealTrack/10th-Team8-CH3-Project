// Fill out your copyright notice in the Description page of Project Settings.


#include "WideRangeAttackSkillComponent.h"

#include "AugmentDamageLibrary.h"
#include "Engine/World.h"
#include "TimerManager.h"

//생성자
UWideRangeAttackSkillComponent::UWideRangeAttackSkillComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    bWideRangeAttack = false;
}

//범위 공격 증강을 활성화하고 발동 타이머를 시작
void UWideRangeAttackSkillComponent::ExecuteWideRangeAttack()
{
    if (bWideRangeAttack)
    {
        return;
    }

    bWideRangeAttack = true;

    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        WideRangeAttackTimerHandle,
        this,
        &UWideRangeAttackSkillComponent::ProcessWideRangeAttackTick,
        AREA_ATTACK_INTERVAL,
        true
    );
}

//증강을 보유 중인지 여부
bool UWideRangeAttackSkillComponent::HasWideRangeAttack()
{
    return bWideRangeAttack;
}

//범위 안의 대상 목록과 줄 데미지를 계산 실제 적용은 하지 않음
void UWideRangeAttackSkillComponent::GatherDamageTargets(TArray<AActor*>& OutTargets, float& OutDamage)
{
    AActor* OwnerActor = GetOwner();

    UAugmentDamageLibrary::FindTargetsInRadius(OwnerActor, AREA_ATTACK_RADIUS, OutTargets);

    OutDamage = UAugmentDamageLibrary::GetOutgoingDamage(OwnerActor) * AREA_ATTACK_DAMAGE_RATIO;
}

//주변 대상에게 범위 공격 데미지를 적용
void UWideRangeAttackSkillComponent::ProcessWideRangeAttackTick()
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
void UWideRangeAttackSkillComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    UWorld* World = GetWorld();

    if (World)
    {
        World->GetTimerManager().ClearTimer(WideRangeAttackTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}
