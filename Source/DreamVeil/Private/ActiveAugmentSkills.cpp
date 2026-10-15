//지속 공격 총에 맞은 대상에게 독을 걺 이미 걸린 대상이면 시간과 데미지만 갱신
    //이미 독에 걸려 있으면 틱 주기는 그대로 두고 남은 시간과 데미지만 새로 맞음
    //불이 붙었다는 연출 켜기 꺼지는 건 타이머가 끝날 때 ProcessPoisonTick이 함
    //첫 독 데미지는 맞은 순간이 아니라 한 간격 뒤부터
//지속 공격 독 타이머 전부 정리
//지속 공격 한 대상에게 독 데미지 한 번
    //독 데미지 표식을 달아서 대상 방어력을 무시하고 틱마다 흡혈이나 가시 갑옷 반사가 걸리지 않음
#include "ActiveAugmentSkills.h"
#include "Particles/ParticleSystem.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "DreamObjectPoolSubsystem.h"

#include "AugmentDamageLibrary.h"
#include "CombatStatsComponent.h"
#include "DispatchTableComponent.h"
#include "WeaponBase.h"
#include "AugmentTypes.h"
#include "DrawDebugHelpers.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "MonsterBase.h"
#include "DreamAugmentRuntime.h"
#include "DreamAugmentFeedback.h"

//감속 중인 대상과 상태 모든 감속 스킬 객체가 공유
//감속 대상이 3초 안에 죽을 수 있어서 약한 참조 사용
//넉백 총에 맞은 적을 쏜 사람 반대쪽으로 살짝 밀어냄
UKnockbackSkill::UKnockbackSkill()
{
    static ConstructorHelpers::FObjectFinderOptional<UParticleSystem> Effect(TEXT("/Game/Effects/Augments/P_KnockbackElectricHit"));
    ElectricHit=Effect.Get();
}

void UKnockbackSkill::OnWeaponHit(const FHitResult& HitResult, float HitDamage)
{
    ACharacter* TargetCharacter = Cast<ACharacter>(HitResult.GetActor());

    if (!TargetCharacter || HitDamage<=0.f)
    {
        return;
    }

    AActor* OwnerActor = GetOwnerActor();

    if (!OwnerActor || TargetCharacter == OwnerActor)
    {
        return;
    }

    auto* Monster=Cast<AMonsterBase>(TargetCharacter);
    if (!Monster || !Monster->TryApplyAugmentStagger(.2f)) return;
    UDreamAugmentFeedback::Emit(OwnerActor,EAugmentID::Knockback,EDreamAugmentCueShape::Impact,HitResult.ImpactPoint,55,.3f);
    const float Now=GetWorld()?GetWorld()->GetTimeSeconds():0.f;
    if (ElectricHit && GetWorld() && GetWorld()->GetNetMode()!=NM_DedicatedServer && (LastElectricTime<0.f || Now-LastElectricTime>=.1f))
    {
        LastElectricTime=Now;
        UGameplayStatics::SpawnEmitterAttached(ElectricHit,TargetCharacter->GetRootComponent(),NAME_None,HitResult.ImpactPoint+HitResult.ImpactNormal*12.f,
            FRotator::ZeroRotator,FVector(1),EAttachLocation::KeepWorldPosition,true,EPSCPoolMethod::AutoRelease,true);
    }
}

//범위 공격 총알이 맞은 지점 주변 대상에게 데미지
void UAreaAttackSkill::OnWeaponHit(const FHitResult& HitResult, float HitDamage)
{
    AActor* OwnerActor = GetOwnerActor();

    if (!OwnerActor)
    {
        return;
    }

    auto* Runtime=GetOwnerComponent()?GetOwnerComponent()->FindRuntime():nullptr;
    if(Runtime&&Runtime->ShotSkill==EAugmentID::SkillExplosion)return;
    const float SplashDamage = HitDamage * AREA_ATTACK_DAMAGE_RATIO*(Runtime?Runtime->Power():1.f);

    if (SplashDamage <= 0.0f)
    {
        return;
    }

    UDreamAugmentFeedback::Emit(OwnerActor,EAugmentID::AreaAttack,EDreamAugmentCueShape::Ring,HitResult.ImpactPoint+FVector(0,0,8),AREA_ATTACK_RADIUS*(GetOwnerComponent()?GetOwnerComponent()->GetEffectRangeMultiplier():1.f),.4f);

    //쏜 사람은 자기 범위 공격에 안 맞고 직접 맞은 대상은 이미 총 데미지를 받았으므로 제외
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(OwnerActor);

    if (HitResult.GetActor())
    {
        ActorsToIgnore.Add(HitResult.GetActor());
    }

    TArray<AActor*> FoundTargets;

    UAugmentDamageLibrary::FindTargetsAtLocation(OwnerActor, HitResult.ImpactPoint, AREA_ATTACK_RADIUS * (GetOwnerComponent()?GetOwnerComponent()->GetEffectRangeMultiplier():1.f), ActorsToIgnore, FoundTargets);

    //쏜 사람의 아군은 범위 공격에 맞지 않음 보스의 범위 공격이 주변 몬스터를 때리지 않도록
    FoundTargets.RemoveAll([OwnerActor](AActor* FoundTarget)
        {
            return !UAugmentDamageLibrary::IsEnemy(OwnerActor, FoundTarget);
        });

    //터진 자리와 반경을 잠깐 그림 0명이 걸렸을 때 반경이 좁은 건지 주변에 아무도 없던 건지 바로 구분됨
    //대상이 없어도 그려야 확인이 되므로 아래 조기 반환보다 먼저 함
    if (AREA_ATTACK_DRAW_DEBUG)
    {
        //데미지가 들어가는 범위를 그대로 그림 빨간 구체 안에 있는 적이 맞는 것
        DrawDebugSphere(OwnerActor->GetWorld(), HitResult.ImpactPoint, AREA_ATTACK_RADIUS*(GetOwnerComponent()?GetOwnerComponent()->GetEffectRangeMultiplier():1.f), 16, FColor::Red, false, 0.4f);

        UE_LOG(LogTemp, Warning, TEXT("[AreaAttack] 반경 %.0f 안에서 %d명 적중 대상당 데미지 %.1f"),
            AREA_ATTACK_RADIUS, FoundTargets.Num(), SplashDamage);
    }

    if (FoundTargets.Num() == 0)
    {
        return;
    }

    //데미지보다 먼저 재생 이번 폭발로 죽어서 사라져도 맞은 자리에 피는 튀게
    //무기 연출을 빌려 쓰는 이유 직격과 범위 피해의 피가 달라 보이면 같은 총에 맞은 것 같지 않음
    UWeaponBase* ActiveWeapon = UWeaponBase::FindActiveWeapon(OwnerActor);

    for (AActor* FoundTarget : FoundTargets)
    {
        if (ActiveWeapon)
        {
            ActiveWeapon->PlaySplashHitEffect(FoundTarget->GetActorLocation(), HitResult.ImpactPoint);
        }

        //터진 자리에서 멀수록 약하게 맞음
        //거리를 반경으로 나눠서 0~1을 만들고 1에서 빼면 가까울수록 큰 값이 됨
        //균일하게 넣으면 가장자리에 걸친 적도 똑같이 아파서 어디에 쏠지 고민할 이유가 없어짐
        const float DistanceToCenter = FVector::Dist(FoundTarget->GetActorLocation(), HitResult.ImpactPoint);
        const float CenterRatio = 1.0f - FMath::Clamp(DistanceToCenter / (AREA_ATTACK_RADIUS * (GetOwnerComponent()?GetOwnerComponent()->GetEffectRangeMultiplier():1.f)), 0.0f, 1.0f);

        //0까지 떨어뜨리지 않고 최소 비율을 깔아둠 가장자리도 맞았다는 느낌은 나야 함
        const float FalloffRatio = FMath::Lerp(AREA_ATTACK_MIN_FALLOFF, 1.0f, CenterRatio);

        //범위 데미지는 ApplyWeaponHit를 거치지 않으므로 범위 공격이 다시 터지지 않음
        //대상마다 값이 달라서 여러 명에게 한 번에 보내는 ApplyAugmentDamage를 쓰지 않음
        UAugmentDamageLibrary::ApplyAugmentDamageToTarget(OwnerActor, FoundTarget, SplashDamage * FalloffRatio);
    }
}

//지속 공격 총에 맞은 대상에게 불을 붙임 이미 걸린 대상이면 시간과 데미지만 갱신
void UContinuousAttackSkill::OnWeaponHit(const FHitResult& HitResult, float HitDamage)
{
    if(auto* Runtime=GetOwnerComponent()?GetOwnerComponent()->FindRuntime():nullptr)
    {
        if(HitResult.GetActor()&&UAugmentDamageLibrary::IsEnemy(GetOwnerActor(),HitResult.GetActor()))Runtime->Burn(HitResult.GetActor(),HitDamage,CONTINUOUS_ATTACK_DURATION*Runtime->Duration());
        return;
    }
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
    if (BurnState && BurnState->TargetGeneration != UDreamObjectPoolSubsystem::Generation(TargetActor))
    {
        World->GetTimerManager().ClearTimer(BurnState->TickTimerHandle);
        BurningTargets.Remove(TargetActor); BurnState = nullptr;
    }
    if (BurnState)
    {
        BurnState->TickDamage = TickDamage;
        BurnState->RemainingTime = CONTINUOUS_ATTACK_DURATION;
        return;
    }

    FBurningTargetState NewState;
    NewState.TickDamage = TickDamage;
    NewState.TargetGeneration = UDreamObjectPoolSubsystem::Generation(TargetActor);
    NewState.RemainingTime = CONTINUOUS_ATTACK_DURATION;

    BurnState = &BurningTargets.Add(TargetActor, NewState);

    //불이 붙었다는 연출 켜기 꺼지는 건 타이머가 끝날 때 ProcessBurnTick이 함
    //이미 걸려 있던 대상은 위에서 돌아가므로 여기까지 오지 않아 불꽃이 겹치지 않음
    if (UCombatStatsComponent* TargetStats = TargetActor->FindComponentByClass<UCombatStatsComponent>())
    {
        //불꽃 에셋은 불을 붙인 쪽의 증강 컴포넌트가 들고 있음 몬스터 블루프린트마다 넣지 않아도 됨
        //불타는 상태 자체는 대상 본인이 들고 있어야 해서 상태만 대상 쪽에 켬
        UDispatchTableComponent* OwnerTable = GetOwnerActor() ? GetOwnerActor()->FindComponentByClass<UDispatchTableComponent>() : nullptr;

        TargetStats->SetOnFire(true, OwnerTable ? OwnerTable->OnFireEffect.Get() : nullptr);
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
        if (AActor* BurningActor = BurnPair.Key.Get(); BurningActor && BurnPair.Value.TargetGeneration == UDreamObjectPoolSubsystem::Generation(BurningActor))
        {
            if (UCombatStatsComponent* BurningStats = BurningActor->FindComponentByClass<UCombatStatsComponent>())
            {
                BurningStats->SetOnFire(false, nullptr);
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
    if (!TargetActor || BurnState->TargetGeneration != UDreamObjectPoolSubsystem::Generation(TargetActor))
    {
        if (World) World->GetTimerManager().ClearTimer(BurnState->TickTimerHandle);
        BurningTargets.Remove(WeakTarget); return;
    }


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
            TargetStats->SetOnFire(false, nullptr);
        }
    }

    BurningTargets.Remove(WeakTarget);
}
