#include "ActiveAugmentSkills.h"

#include "AugmentDamageLibrary.h"
#include "AugmentTypes.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

//감속 발동 타이머 시작 이미 돌고 있으면 무시
void USlowEnemySkill::Apply()
{
    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    if (World->GetTimerManager().IsTimerActive(SlowTimerHandle))
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        SlowTimerHandle,
        this,
        &USlowEnemySkill::ProcessSlowTick,
        SLOW_ENEMY_INTERVAL,
        true
    );
}

//감속 느려진 대상을 되돌리고 타이머 정리
void USlowEnemySkill::Deactivate()
{
    //느려진 대상을 그대로 두고 사라지지 않도록 정리
    RestoreSlowedCharacters();

    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    World->GetTimerManager().ClearTimer(SlowTimerHandle);
    World->GetTimerManager().ClearTimer(RestoreTimerHandle);
}

//감속 주변 대상의 이동 속도를 낮춤
void USlowEnemySkill::ProcessSlowTick()
{
    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    //이전에 걸린 속도 저하가 남아 있으면 먼저 되돌림
    RestoreSlowedCharacters();

    TArray<AActor*> FoundTargets;

    UAugmentDamageLibrary::FindTargetsInRadius(GetOwnerActor(), SLOW_ENEMY_RADIUS, FoundTargets);

    for (AActor* Target : FoundTargets)
    {
        ACharacter* TargetCharacter = Cast<ACharacter>(Target);

        if (!TargetCharacter)
        {
            continue;
        }

        UCharacterMovementComponent* MovementComponent = TargetCharacter->GetCharacterMovement();

        if (!MovementComponent)
        {
            continue;
        }

        //같은 대상을 두 번 느리게 만들어 원래 속도를 잃어버리는 것을 막음
        if (SlowedCharacters.Contains(TargetCharacter))
        {
            continue;
        }

        SlowedCharacters.Add(TargetCharacter, MovementComponent->MaxWalkSpeed);

        MovementComponent->MaxWalkSpeed *= SLOW_ENEMY_RATIO;
    }

    if (SlowedCharacters.Num() == 0)
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        RestoreTimerHandle,
        this,
        &USlowEnemySkill::RestoreSlowedCharacters,
        SLOW_ENEMY_DURATION,
        false
    );
}

//감속 속도가 저하된 대상을 원래 속도로 되돌림
void USlowEnemySkill::RestoreSlowedCharacters()
{
    for (TPair<TWeakObjectPtr<ACharacter>, float>& SlowedPair : SlowedCharacters)
    {
        ACharacter* SlowedCharacter = SlowedPair.Key.Get();

        if (!SlowedCharacter)
        {
            continue;
        }

        UCharacterMovementComponent* MovementComponent = SlowedCharacter->GetCharacterMovement();

        if (!MovementComponent)
        {
            continue;
        }

        MovementComponent->MaxWalkSpeed = SlowedPair.Value;
    }

    SlowedCharacters.Empty();
}

//범위 공격 발동 타이머 시작 이미 돌고 있으면 무시
void UAreaAttackSkill::Apply()
{
    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    if (World->GetTimerManager().IsTimerActive(AreaAttackTimerHandle))
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        AreaAttackTimerHandle,
        this,
        &UAreaAttackSkill::ProcessAreaAttackTick,
        AREA_ATTACK_INTERVAL,
        true
    );
}

//범위 공격 타이머 정리
void UAreaAttackSkill::Deactivate()
{
    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    World->GetTimerManager().ClearTimer(AreaAttackTimerHandle);
}

//범위 공격 주변 대상 전원에게 데미지
void UAreaAttackSkill::ProcessAreaAttackTick()
{
    AActor* OwnerActor = GetOwnerActor();

    TArray<AActor*> FoundTargets;

    UAugmentDamageLibrary::FindTargetsInRadius(OwnerActor, AREA_ATTACK_RADIUS, FoundTargets);

    if (FoundTargets.Num() == 0)
    {
        return;
    }

    const float Damage = UAugmentDamageLibrary::GetOutgoingDamage(OwnerActor) * AREA_ATTACK_DAMAGE_RATIO;

    if (Damage <= 0.0f)
    {
        return;
    }

    UAugmentDamageLibrary::ApplyAugmentDamage(OwnerActor, FoundTargets, Damage);
}

//지속 공격 발동 타이머 시작 이미 돌고 있으면 무시
void UContinuousAttackSkill::Apply()
{
    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    if (World->GetTimerManager().IsTimerActive(ContinuousAttackTimerHandle))
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        ContinuousAttackTimerHandle,
        this,
        &UContinuousAttackSkill::ProcessContinuousAttackTick,
        CONTINUOUS_ATTACK_INTERVAL,
        true
    );
}

//지속 공격 타이머 정리
void UContinuousAttackSkill::Deactivate()
{
    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    World->GetTimerManager().ClearTimer(ContinuousAttackTimerHandle);
}

//지속 공격 주변 대상 전원에게 데미지
void UContinuousAttackSkill::ProcessContinuousAttackTick()
{
    AActor* OwnerActor = GetOwnerActor();

    TArray<AActor*> FoundTargets;

    UAugmentDamageLibrary::FindTargetsInRadius(OwnerActor, CONTINUOUS_ATTACK_RADIUS, FoundTargets);

    if (FoundTargets.Num() == 0)
    {
        return;
    }

    const float Damage = UAugmentDamageLibrary::GetOutgoingDamage(OwnerActor) * CONTINUOUS_ATTACK_DAMAGE_RATIO;

    if (Damage <= 0.0f)
    {
        return;
    }

    UAugmentDamageLibrary::ApplyAugmentDamage(OwnerActor, FoundTargets, Damage);
}
