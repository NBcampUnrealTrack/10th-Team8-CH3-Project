//지속 공격 총에 맞은 대상에게 독을 걺 이미 걸린 대상이면 시간과 데미지만 갱신
    //이미 독에 걸려 있으면 틱 주기는 그대로 두고 남은 시간과 데미지만 새로 맞음
    //불이 붙었다는 연출 켜기 꺼지는 건 타이머가 끝날 때 ProcessPoisonTick이 함
    //첫 독 데미지는 맞은 순간이 아니라 한 간격 뒤부터
//지속 공격 독 타이머 전부 정리
//지속 공격 한 대상에게 독 데미지 한 번
    //독 데미지 표식을 달아서 대상 방어력을 무시하고 틱마다 흡혈이나 가시 갑옷 반사가 걸리지 않음
#include "ActiveAugmentSkills.h"

#include "AugmentDamageLibrary.h"
#include "CombatStatsComponent.h"
#include "AugmentTypes.h"
#include "DrawDebugHelpers.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

//감속 중인 대상과 상태 모든 감속 스킬 객체가 공유
//감속 대상이 3초 안에 죽을 수 있어서 약한 참조 사용
FSlowedCharacterMap USlowEnemySkill::SlowedCharacters;

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
    //순회 중에 지워야 해서 range-for 대신 반복자를 씀 RemoveCurrent는 지금 칸을 지우고 안전하게 다음으로 넘어감
    for (FSlowedCharacterMap::TIterator It = SlowedCharacters.CreateIterator(); It; ++It)
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

    //주변에 아무도 없어도 터지는 건 보여야 해서 대상을 찾기 전에 먼저 재생함
    //이펙트 에셋은 쏜 사람의 스탯 컴포넌트가 들고 있음 증강 스킬은 UObject라 에셋을 못 들고 있어서
    if (UCombatStatsComponent* OwnerStats = OwnerActor->FindComponentByClass<UCombatStatsComponent>())
    {
        OwnerStats->PlayAreaAttackEffect(HitResult.ImpactPoint);
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

    //터진 자리와 반경을 잠깐 그림 0명이 걸렸을 때 반경이 좁은 건지 주변에 아무도 없던 건지 바로 구분됨
    //대상이 없어도 그려야 확인이 되므로 아래 조기 반환보다 먼저 함
    if (AREA_ATTACK_DRAW_DEBUG)
    {
        DrawDebugSphere(OwnerActor->GetWorld(), HitResult.ImpactPoint, AREA_ATTACK_RADIUS, 16, FColor::Orange, false, 1.0f);

        UE_LOG(LogTemp, Warning, TEXT("[AreaAttack] 반경 %.0f 안에서 %d명 적중 대상당 데미지 %.1f"),
            AREA_ATTACK_RADIUS, FoundTargets.Num(), SplashDamage);
    }

    if (FoundTargets.Num() == 0)
    {
        return;
    }

    //범위 데미지는 ApplyWeaponHit를 거치지 않으므로 범위 공격이 다시 터지지 않음
    UAugmentDamageLibrary::ApplyAugmentDamage(OwnerActor, FoundTargets, SplashDamage);
}

//지속 공격 총에 맞은 대상에게 불을 붙임 이미 걸린 대상이면 시간과 데미지만 갱신
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

    FBurningTargetState* BurnState = BurningTargets.Find(TargetActor);

    //이미 불이 붙어 있으면 틱 주기는 그대로 두고 남은 시간과 데미지만 새로 맞음
    if (BurnState)
    {
        BurnState->TickDamage = TickDamage;
        BurnState->RemainingTime = CONTINUOUS_ATTACK_DURATION;
        return;
    }

    FBurningTargetState NewState;
    NewState.TickDamage = TickDamage;
    NewState.RemainingTime = CONTINUOUS_ATTACK_DURATION;

    BurnState = &BurningTargets.Add(TargetActor, NewState);

    //불이 붙었다는 연출 켜기 꺼지는 건 타이머가 끝날 때 ProcessBurnTick이 함
    //이미 걸려 있던 대상은 위에서 돌아가므로 여기까지 오지 않아 불꽃이 겹치지 않음
    if (UCombatStatsComponent* TargetStats = TargetActor->FindComponentByClass<UCombatStatsComponent>())
    {
        TargetStats->SetOnFire(true);
    }

    //첫 화염 데미지는 맞은 순간이 아니라 한 간격 뒤부터
    FTimerDelegate TickDelegate = FTimerDelegate::CreateUObject(
        this,
        &UContinuousAttackSkill::ProcessBurnTick,
        TWeakObjectPtr<AActor>(TargetActor)
    );

    World->GetTimerManager().SetTimer(BurnState->TickTimerHandle, TickDelegate, CONTINUOUS_ATTACK_INTERVAL, true);
}

//지속 공격 화염 타이머 전부 정리
void UContinuousAttackSkill::Deactivate()
{
    UWorld* World = GetWorld();

    for (TPair<TWeakObjectPtr<AActor>, FBurningTargetState>& BurnPair : BurningTargets)
    {
        if (World)
        {
            World->GetTimerManager().ClearTimer(BurnPair.Value.TickTimerHandle);
        }

        //타이머만 지우면 불꽃이 영원히 남으므로 여기서도 꺼줌
        //약한 참조라 이미 사라진 대상은 Get이 nullptr을 돌려줘서 자동으로 걸러짐
        if (AActor* BurningActor = BurnPair.Key.Get())
        {
            if (UCombatStatsComponent* BurningStats = BurningActor->FindComponentByClass<UCombatStatsComponent>())
            {
                BurningStats->SetOnFire(false);
            }
        }
    }

    BurningTargets.Empty();
}

//지속 공격 한 대상에게 화염 데미지 한 번
void UContinuousAttackSkill::ProcessBurnTick(TWeakObjectPtr<AActor> WeakTarget)
{
    FBurningTargetState* BurnState = BurningTargets.Find(WeakTarget);

    if (!BurnState)
    {
        return;
    }

    UWorld* World = GetWorld();

    AActor* TargetActor = WeakTarget.Get();

    //화염 데미지 표식을 달아서 대상 방어력을 무시하고 틱마다 흡혈이나 가시 갑옷 반사가 걸리지 않음
    //대상이 사라졌거나 이미 죽어서 데미지가 안 들어가면 0이 돌아옴
    const float AppliedDamage = UAugmentDamageLibrary::ApplyFireDamage(GetOwnerActor(), TargetActor, BurnState->TickDamage);

    //데미지 처리 중에 목록이 바뀌었을 수 있으니 다시 찾음
    BurnState = BurningTargets.Find(WeakTarget);

    if (!BurnState)
    {
        return;
    }

    BurnState->RemainingTime -= CONTINUOUS_ATTACK_INTERVAL;

    const bool bExpired = BurnState->RemainingTime <= KINDA_SMALL_NUMBER;

    if (!bExpired && AppliedDamage > 0.0f)
    {
        return;
    }

    if (World)
    {
        World->GetTimerManager().ClearTimer(BurnState->TickTimerHandle);
    }

    //시간이 다 됐거나 대상이 죽었으므로 불을 끔
    //대상이 이미 사라졌으면 TargetActor가 nullptr이라 건너뜀 이때는 이펙트도 같이 사라져 있음
    if (TargetActor)
    {
        if (UCombatStatsComponent* TargetStats = TargetActor->FindComponentByClass<UCombatStatsComponent>())
        {
            TargetStats->SetOnFire(false);
        }
    }

    BurningTargets.Remove(WeakTarget);
}
