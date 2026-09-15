#include "ActiveAugmentSkills.h"

#include "AugmentDamageLibrary.h"
#include "AugmentTypes.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

//감속 중인 대상과 상태 모든 감속 스킬 객체가 공유
//감속 대상이 3초 안에 죽을 수 있어서 약한 참조 사용
TMap<TWeakObjectPtr<ACharacter>, FSlowedCharacterState> USlowEnemySkill::SlowedCharacters;

//감속 총에 맞은 캐릭터를 느리게 만듦 이미 느린 대상이면 누가 걸었든 시간만 다시 시작
void USlowEnemySkill::OnWeaponHit(const FHitResult& HitResult, float HitDamage)
{
    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    ACharacter* TargetCharacter = Cast<ACharacter>(HitResult.GetActor());

    if (!TargetCharacter)
    {
        return;
    }

    if (TargetCharacter == GetOwnerActor())
    {
        return;
    }

    UCharacterMovementComponent* MovementComponent = TargetCharacter->GetCharacterMovement();

    if (!MovementComponent)
    {
        return;
    }

    FSlowedCharacterState* SlowedState = SlowedCharacters.Find(TargetCharacter);

    //처음 느려지는 대상만 원래 속도를 저장하고 배율을 곱함 다른 공격자에게 다시 맞아도 더 느려지지 않음
    if (!SlowedState)
    {
        RemoveInvalidTargets();

        FSlowedCharacterState NewState;
        NewState.OriginalWalkSpeed = MovementComponent->MaxWalkSpeed;

        MovementComponent->MaxWalkSpeed *= SLOW_ENEMY_RATIO;

        SlowedState = &SlowedCharacters.Add(TargetCharacter, NewState);
    }

    //같은 핸들로 다시 걸면 이전 타이머는 취소되고 지속 시간이 처음부터 다시 흐름
    //스킬 객체에 묶지 않은 정적 함수라서 쏜 사람이 사라져 스킬이 정리돼도 타이머는 끝까지 돌아 속도를 되돌림
    FTimerDelegate RestoreDelegate = FTimerDelegate::CreateStatic(
        &USlowEnemySkill::RestoreCharacter,
        TWeakObjectPtr<ACharacter>(TargetCharacter)
    );

    World->GetTimerManager().SetTimer(SlowedState->RestoreTimerHandle, RestoreDelegate, SLOW_ENEMY_DURATION, false);
}

//이미 사라진 대상의 상태를 목록에서 지움
void USlowEnemySkill::RemoveInvalidTargets()
{
    for (auto It = SlowedCharacters.CreateIterator(); It; ++It)
    {
        if (!It->Key.IsValid())
        {
            It.RemoveCurrent();
        }
    }
}

//감속 한 대상의 이동 속도를 원래대로 되돌림
void USlowEnemySkill::RestoreCharacter(TWeakObjectPtr<ACharacter> WeakTarget)
{
    FSlowedCharacterState* SlowedState = SlowedCharacters.Find(WeakTarget);

    if (!SlowedState)
    {
        return;
    }

    ACharacter* SlowedCharacter = WeakTarget.Get();

    if (SlowedCharacter)
    {
        UCharacterMovementComponent* MovementComponent = SlowedCharacter->GetCharacterMovement();

        if (MovementComponent)
        {
            MovementComponent->MaxWalkSpeed = SlowedState->OriginalWalkSpeed;
        }
    }

    //대상이 이미 사라졌어도 목록에서는 지움
    SlowedCharacters.Remove(WeakTarget);
}

//범위 공격 총알이 맞은 지점 주변 대상에게 데미지
void UAreaAttackSkill::OnWeaponHit(const FHitResult& HitResult, float HitDamage)
{
    AActor* OwnerActor = GetOwnerActor();

    if (!OwnerActor)
    {
        return;
    }

    const float SplashDamage = HitDamage * AREA_ATTACK_DAMAGE_RATIO;

    if (SplashDamage <= 0.0f)
    {
        return;
    }

    //쏜 사람은 자기 범위 공격에 안 맞고 직접 맞은 대상은 이미 총 데미지를 받았으므로 제외
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(OwnerActor);

    if (HitResult.GetActor())
    {
        ActorsToIgnore.Add(HitResult.GetActor());
    }

    TArray<AActor*> FoundTargets;

    UAugmentDamageLibrary::FindTargetsAtLocation(OwnerActor, HitResult.ImpactPoint, AREA_ATTACK_RADIUS, ActorsToIgnore, FoundTargets);

    //쏜 사람의 아군은 범위 공격에 맞지 않음 보스의 범위 공격이 주변 몬스터를 때리지 않도록
    FoundTargets.RemoveAll([OwnerActor](AActor* FoundTarget)
        {
            return !UAugmentDamageLibrary::IsEnemy(OwnerActor, FoundTarget);
        });

    if (FoundTargets.Num() == 0)
    {
        return;
    }

    //범위 데미지는 ApplyWeaponHit를 거치지 않으므로 범위 공격이 다시 터지지 않음
    UAugmentDamageLibrary::ApplyAugmentDamage(OwnerActor, FoundTargets, SplashDamage);
}

//지속 공격 총에 맞은 대상에게 독을 걺 이미 걸린 대상이면 시간과 데미지만 갱신
void UContinuousAttackSkill::OnWeaponHit(const FHitResult& HitResult, float HitDamage)
{
    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    AActor* TargetActor = HitResult.GetActor();

    if (!TargetActor)
    {
        return;
    }

    if (TargetActor == GetOwnerActor())
    {
        return;
    }

    const float TickDamage = HitDamage * CONTINUOUS_ATTACK_DAMAGE_RATIO;

    if (TickDamage <= 0.0f)
    {
        return;
    }

    FPoisonedTargetState* PoisonState = PoisonedTargets.Find(TargetActor);

    //이미 독에 걸려 있으면 틱 주기는 그대로 두고 남은 시간과 데미지만 새로 맞음
    if (PoisonState)
    {
        PoisonState->TickDamage = TickDamage;
        PoisonState->RemainingTime = CONTINUOUS_ATTACK_DURATION;
        return;
    }

    FPoisonedTargetState NewState;
    NewState.TickDamage = TickDamage;
    NewState.RemainingTime = CONTINUOUS_ATTACK_DURATION;

    PoisonState = &PoisonedTargets.Add(TargetActor, NewState);

    //첫 독 데미지는 맞은 순간이 아니라 한 간격 뒤부터
    FTimerDelegate TickDelegate = FTimerDelegate::CreateUObject(
        this,
        &UContinuousAttackSkill::ProcessPoisonTick,
        TWeakObjectPtr<AActor>(TargetActor)
    );

    World->GetTimerManager().SetTimer(PoisonState->TickTimerHandle, TickDelegate, CONTINUOUS_ATTACK_INTERVAL, true);
}

//지속 공격 독 타이머 전부 정리
void UContinuousAttackSkill::Deactivate()
{
    UWorld* World = GetWorld();

    if (World)
    {
        for (TPair<TWeakObjectPtr<AActor>, FPoisonedTargetState>& PoisonPair : PoisonedTargets)
        {
            World->GetTimerManager().ClearTimer(PoisonPair.Value.TickTimerHandle);
        }
    }

    PoisonedTargets.Empty();
}

//지속 공격 한 대상에게 독 데미지 한 번
void UContinuousAttackSkill::ProcessPoisonTick(TWeakObjectPtr<AActor> WeakTarget)
{
    FPoisonedTargetState* PoisonState = PoisonedTargets.Find(WeakTarget);

    if (!PoisonState)
    {
        return;
    }

    UWorld* World = GetWorld();

    AActor* TargetActor = WeakTarget.Get();

    //독 데미지 표식을 달아서 대상 방어력을 무시하고 틱마다 흡혈이나 가시 갑옷 반사가 걸리지 않음
    //대상이 사라졌거나 이미 죽어서 데미지가 안 들어가면 0이 돌아옴
    const float AppliedDamage = UAugmentDamageLibrary::ApplyPoisonDamage(GetOwnerActor(), TargetActor, PoisonState->TickDamage);

    //데미지 처리 중에 목록이 바뀌었을 수 있으니 다시 찾음
    PoisonState = PoisonedTargets.Find(WeakTarget);

    if (!PoisonState)
    {
        return;
    }

    PoisonState->RemainingTime -= CONTINUOUS_ATTACK_INTERVAL;

    const bool bExpired = PoisonState->RemainingTime <= KINDA_SMALL_NUMBER;

    if (!bExpired && AppliedDamage > 0.0f)
    {
        return;
    }

    if (World)
    {
        World->GetTimerManager().ClearTimer(PoisonState->TickTimerHandle);
    }

    PoisonedTargets.Remove(WeakTarget);
}
