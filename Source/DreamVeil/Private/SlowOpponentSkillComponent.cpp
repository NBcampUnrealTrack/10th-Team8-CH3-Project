// Fill out your copyright notice in the Description page of Project Settings.


#include "SlowOpponentSkillComponent.h"

#include "AugmentDamageLibrary.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

//생성자
USlowOpponentSkillComponent::USlowOpponentSkillComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    bSlowOpponent = false;
}

//속도 저하 증강을 활성화하고 발동 타이머를 시작
void USlowOpponentSkillComponent::ExecuteSlowOpponent()
{
    if (bSlowOpponent)
    {
        return;
    }

    bSlowOpponent = true;

    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        SlowTimerHandle,
        this,
        &USlowOpponentSkillComponent::ProcessSlowTick,
        SLOW_ENEMY_INTERVAL,
        true
    );
}

//증강을 보유 중인지 여부
bool USlowOpponentSkillComponent::HasSlowOpponent()
{
    return bSlowOpponent;
}

//주변 대상의 이동 속도를 낮춤
void USlowOpponentSkillComponent::ProcessSlowTick()
{
    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    //이전에 걸린 속도 저하가 남아 있으면 먼저 되돌림
    RestoreSlowedCharacters();

    TArray<AActor*> FoundTargets;

    UAugmentDamageLibrary::FindTargetsInRadius(GetOwner(), SLOW_ENEMY_RADIUS, FoundTargets);

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
        &USlowOpponentSkillComponent::RestoreSlowedCharacters,
        SLOW_ENEMY_DURATION,
        false
    );
}

//속도가 저하된 대상을 원래 속도로 되돌림
void USlowOpponentSkillComponent::RestoreSlowedCharacters()
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

//생명주기 함수
void USlowOpponentSkillComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    //느려진 대상을 그대로 두고 사라지지 않도록 정리
    RestoreSlowedCharacters();

    UWorld* World = GetWorld();

    if (World)
    {
        World->GetTimerManager().ClearTimer(SlowTimerHandle);
        World->GetTimerManager().ClearTimer(RestoreTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}
