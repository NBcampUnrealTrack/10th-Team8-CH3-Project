// Fill out your copyright notice in the Description page of Project Settings.


#include "ActiveSkillsComponent.h"

#include "AttackComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"

//생성자
UActiveSkillsComponent::UActiveSkillsComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    AttackComponent = nullptr;

    bSlowEnemy = false;
    bAreaAttack = false;
    bContinuousAttack = false;
}

//증강 효과 함수들을 테이블에 등록
void UActiveSkillsComponent::RegisterActiveAugmentFunctions()
{
    ActiveAugmentMap.Add(EAugmentID::SlowEnemy, [this]() { ExecuteSlowEnemy(); });
    ActiveAugmentMap.Add(EAugmentID::AreaAttack, [this]() { ExecuteAreaAttack(); });
    ActiveAugmentMap.Add(EAugmentID::ContinuousAttack, [this]() { ExecuteContinuousAttack(); });
}

//번호로 액티브 증강 효과를 실행
void UActiveSkillsComponent::ExecuteActiveAugment(EAugmentID AugmentID)
{
    TFunction<void()>* FoundFunction = ActiveAugmentMap.Find(AugmentID);

    if (!FoundFunction)
    {
        return;
    }

    (*FoundFunction)();
}

//속도 저하 증강을 활성화하고 발동 타이머를 시작
void UActiveSkillsComponent::ExecuteSlowEnemy()
{
    if (bSlowEnemy)
    {
        return;
    }

    bSlowEnemy = true;

    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        SlowEnemyTimerHandle,
        this,
        &UActiveSkillsComponent::ProcessSlowEnemyTick,
        SLOW_ENEMY_INTERVAL,
        true
    );
}

//범위 공격 증강을 활성화하고 발동 타이머를 시작
void UActiveSkillsComponent::ExecuteAreaAttack()
{
    if (bAreaAttack)
    {
        return;
    }

    bAreaAttack = true;

    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        AreaAttackTimerHandle,
        this,
        &UActiveSkillsComponent::ProcessAreaAttackTick,
        AREA_ATTACK_INTERVAL,
        true
    );
}

//지속 공격 증강을 활성화하고 발동 타이머를 시작
void UActiveSkillsComponent::ExecuteContinuousAttack()
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
        &UActiveSkillsComponent::ProcessContinuousAttackTick,
        CONTINUOUS_ATTACK_INTERVAL,
        true
    );
}

//주변 대상의 이동 속도를 낮춤
void UActiveSkillsComponent::ProcessSlowEnemyTick()
{
    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    //이전에 걸린 속도 저하가 남아 있으면 먼저 되돌림
    RestoreSlowedCharacters();

    TArray<AActor*> FoundTargets;

    FindTargetsInRadius(SLOW_ENEMY_RADIUS, FoundTargets);

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
        SlowEnemyRestoreTimerHandle,
        this,
        &UActiveSkillsComponent::RestoreSlowedCharacters,
        SLOW_ENEMY_DURATION,
        false
    );
}

//속도가 저하된 대상을 원래 속도로 되돌림
void UActiveSkillsComponent::RestoreSlowedCharacters()
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

//주변 대상에게 줄 범위 공격 데미지를 알림
void UActiveSkillsComponent::ProcessAreaAttackTick()
{
    RequestDamageInRadius(AREA_ATTACK_RADIUS, AREA_ATTACK_DAMAGE_RATIO);
}

//주변 대상에게 줄 지속 공격 데미지를 알림
void UActiveSkillsComponent::ProcessContinuousAttackTick()
{
    RequestDamageInRadius(CONTINUOUS_ATTACK_RADIUS, CONTINUOUS_ATTACK_DAMAGE_RATIO);
}

//지정한 범위 안의 대상을 찾음
void UActiveSkillsComponent::FindTargetsInRadius(float Radius, TArray<AActor*>& OutTargets)
{
    OutTargets.Empty();

    AActor* OwnerActor = GetOwner();

    if (!OwnerActor)
    {
        return;
    }

    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(OwnerActor);

    //지금은 폰 전체를 잡음 아군 적군 구분은 플레이어 및 데미지 쪽이 정리되면 여기서 걸러냄
    UKismetSystemLibrary::SphereOverlapActors(
        this,
        OwnerActor->GetActorLocation(),
        Radius,
        ObjectTypes,
        AActor::StaticClass(),
        ActorsToIgnore,
        OutTargets
    );
}

//지정한 범위 안의 대상에게 줄 데미지를 계산하고 대상 목록을 반환
void UActiveSkillsComponent::GatherDamageTargets(float Radius, float DamageRatio, TArray<AActor*>& OutTargets, float& OutDamage)
{
    OutDamage = 0.0f;

    FindTargetsInRadius(Radius, OutTargets);

    if (!AttackComponent)
    {
        return;
    }

    OutDamage = AttackComponent->GetAttackPower() * DamageRatio;
}

//대상 목록과 데미지를 계산해 이벤트로 알림
void UActiveSkillsComponent::RequestDamageInRadius(float Radius, float DamageRatio)
{
    TArray<AActor*> FoundTargets;
    float Damage = 0.0f;

    GatherDamageTargets(Radius, DamageRatio, FoundTargets, Damage);

    if (FoundTargets.Num() == 0)
    {
        return;
    }

    if (Damage <= 0.0f)
    {
        return;
    }

    OnActiveDamageRequested.Broadcast(FoundTargets, Damage);
}

//생명주기 함수
void UActiveSkillsComponent::BeginPlay()
{
    Super::BeginPlay();

    RegisterActiveAugmentFunctions();

    AActor* OwnerActor = GetOwner();

    if (!OwnerActor)
    {
        return;
    }

    AttackComponent = OwnerActor->FindComponentByClass<UAttackComponent>();
}

void UActiveSkillsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    //느려진 대상을 그대로 두고 사라지지 않도록 정리
    RestoreSlowedCharacters();

    UWorld* World = GetWorld();

    if (World)
    {
        World->GetTimerManager().ClearTimer(SlowEnemyTimerHandle);
        World->GetTimerManager().ClearTimer(SlowEnemyRestoreTimerHandle);
        World->GetTimerManager().ClearTimer(AreaAttackTimerHandle);
        World->GetTimerManager().ClearTimer(ContinuousAttackTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}
