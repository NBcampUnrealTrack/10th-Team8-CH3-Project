#include "MonsterSkill.h"

#include "MonsterBase.h"
#include "CombatStatsComponent.h"
#include "Engine/World.h"
#include "MonsterAIController.h"
#include "MonsterCollision.h"
#include "EliteMonster.h"
#include "BossMonster.h"
#include "MainPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    //요청한 거리와 준비 시간은 고정하고, 피해와 쿨타임만 에디터에서 조절한다.
    constexpr float ChargeTriggerDistance = 800.0f;
    constexpr float ChargeDistance = 1200.0f;
    constexpr float ChargeReadySeconds = 2.0f;
    //초당 1200cm로 1초 동안 돌진한다. 경사와 바닥 충돌은 CharacterMovement가 처리한다.
    constexpr float ChargeSpeed = 1200.0f;
    const FName ChargeMotionName(TEXT("MonsterCharge"));
}

UMonsterSkill::UMonsterSkill()
{
    // 시간 비교로 쿨타임을 확인하므로 Tick이나 반복 타이머는 필요 없다.
    //쿨타임에는 Tick이 필요 없지만 돌진의 구간 충돌 검사에만 잠시 사용한다.
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMonsterSkill::BeginPlay()
{
    Super::BeginPlay();
    if (AMonsterBase* Monster = Cast<AMonsterBase>(GetOwner()))
    {
        //이동이 끝난 뒤 실제로 지나온 구간을 검사해야 벽 뒤까지 공격하지 않는다.
        AddTickPrerequisiteComponent(Monster->GetCharacterMovement());
    }
    if (Cast<AEliteMonster>(GetOwner()) && !Cast<ABossMonster>(GetOwner()))
    {
        GetWorld()->GetTimerManager().SetTimer(ChargeCheckTimer, this, &UMonsterSkill::CheckChargeRange, 0.2f, true);
    }
}

void UMonsterSkill::CheckChargeRange()
{
    //Skills에 Charge를 등록한 몬스터만 실제로 발동한다. 쿨타임 중에는 AI 행동을 건드리지 않는다.
    TryUseSkill(EMonsterSkillType::Charge);
}

const FMonsterSkillSettings* UMonsterSkill::FindSettings(EMonsterSkillType Skill) const
{
    return Skills.FindByPredicate([Skill](const FMonsterSkillSettings& Settings)
    {
        return Settings.Skill == Skill;
    });
}

void UMonsterSkill::SetProgression(int32 Phase, int32 Level)
{
    CurrentPhase = FMath::Max(1, Phase);
    CurrentLevel = FMath::Max(1, Level);
}

float UMonsterSkill::GetCooldownRemaining(EMonsterSkillType Skill) const
{
    const double* NextUseTime = NextUseTimes.Find(Skill);
    return NextUseTime && GetWorld()
        ? static_cast<float>(FMath::Max(0.0, *NextUseTime - GetWorld()->GetTimeSeconds()))
        : 0.0f;
}

bool UMonsterSkill::CanUseSkill(EMonsterSkillType Skill) const
{
    const AMonsterBase* Monster = Cast<AMonsterBase>(GetOwner());
    const FMonsterSkillSettings* Settings = FindSettings(Skill);
    return GetWorld() && IsValid(Monster) && Settings
        && !bIsUsingSkill && !Monster->IsAttacking()
        && Monster->MonsterCombatStats && !Monster->MonsterCombatStats->IsDead()
        && CurrentPhase >= Settings->RequiredPhase
        && CurrentLevel >= Settings->RequiredLevel
        && GetCooldownRemaining(Skill) <= 0.0f;
}

bool UMonsterSkill::BeginSkill(EMonsterSkillType Skill)
{
    if (!CanUseSkill(Skill)) return false;

    ActiveSkill = Skill;
    ActiveCooldown = FMath::Max(0.0f, FindSettings(Skill)->Cooldown);
    bIsUsingSkill = true;
    return true;
}

void UMonsterSkill::FinishSkill()
{
    if (!bIsUsingSkill) return;

    if (GetWorld())
    {
        NextUseTimes.Add(ActiveSkill, GetWorld()->GetTimeSeconds() + ActiveCooldown);
    }
    bIsUsingSkill = false;
    //AI를 재개하기 전에 쿨타임과 사용 상태를 갱신해 즉시 재발동하지 않게 한다.
    CleanupCharge();
}

bool UMonsterSkill::TryUseSkill(EMonsterSkillType Skill)
{
    AMonsterBase* Monster = Cast<AMonsterBase>(GetOwner());
    AMainPlayerCharacter* Player = Cast<AMainPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));

	//스킬 이중체크, 돌진이고 쿨돌았고 플레이어 감지 제대로 됐는지 확인...
    if (Skill != EMonsterSkillType::Charge || !CanUseSkill(Skill) || !IsValid(Player)) return false;
    //죽은 플레이어에게 발동하지 않고, 시전자는 바닥에 서 있을 때만 준비한다.
    if (!Player->CombatStats || Player->CombatStats->IsDead() || !Monster->GetCharacterMovement()->IsMovingOnGround()) return false;

    const FVector ToPlayer = Player->GetActorLocation() - Monster->GetActorLocation();
    if (ToPlayer.SizeSquared() > FMath::Square(ChargeTriggerDistance) || ToPlayer.SizeSquared2D() <= UE_SMALL_NUMBER) return false;
    if (!BeginSkill(Skill)) return false;

    ChargeDirection = ToPlayer.GetSafeNormal2D();
    ChargeStart = Monster->GetActorLocation();
    ChargeHitActors.Reset();
    bChargePrepared = true;
    UCharacterMovementComponent* Movement = Monster->GetCharacterMovement();
    bSavedAvoidance = Movement->bUseRVOAvoidance;
    SavedCapsuleResponses = Monster->GetCapsuleComponent()->GetCollisionResponseToChannels();
    if (AMonsterAIController* AI = Cast<AMonsterAIController>(Monster->GetController()))
    {
        AI->SetSkillMovementLocked(true);
    }
    Movement->StopMovementImmediately();
    Movement->SetAvoidanceEnabled(false);
    Monster->SetActorRotation(ChargeDirection.Rotation());

    //데칼 시작점은 캡슐 중심이 아니라 발밑으로 내려 바닥에 투영한다.
    const FVector WarningStart = ChargeStart - FVector::UpVector * Monster->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    Monster->ShowAttackWarning(WarningStart, WarningStart + ChargeDirection * ChargeDistance,
        Monster->GetCapsuleComponent()->GetScaledCapsuleRadius() * 2.0f);
    PlayChargeAnimation(ChargeReadyAnimation);
    GetWorld()->GetTimerManager().SetTimer(ChargeReadyTimer, this, &UMonsterSkill::StartCharge, ChargeReadySeconds, false);
    return true;
}

void UMonsterSkill::PlayChargeAnimation(UAnimSequence* Animation)
{
    AMonsterBase* Monster = Cast<AMonsterBase>(GetOwner());
    USkeletalMeshComponent* Mesh = Monster ? Cast<USkeletalMeshComponent>(Monster->GetMonsterMesh()) : nullptr;
    if (!Mesh) return;
    //달리기 모션이 비어 있으면 준비용 걷기를 계속 재생하지 않고 원래 AnimBP로 돌린다.
    if (!Animation)
    {
        if (bAnimationOverridden)
        {
            Mesh->AnimationData = SavedAnimationData;
            Mesh->SetAnimationMode(SavedAnimationMode);
            bAnimationOverridden = false;
        }
        return;
    }
    if (!bAnimationOverridden)
    {
        SavedAnimationMode = Mesh->GetAnimationMode();
        SavedAnimationData = Mesh->AnimationData;
        bAnimationOverridden = true;
    }
    Mesh->PlayAnimation(Animation, true);
    //모션에 루트 이동이 있어도 실제 돌진 거리와 준비 위치가 바뀌지 않도록 추출만 하고 무시한다.
    if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
    {
        AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
    }
}

void UMonsterSkill::StartCharge()
{
    AMonsterBase* Monster = Cast<AMonsterBase>(GetOwner());
    if (!bIsUsingSkill || !bChargePrepared || !IsValid(Monster)) return;
    if (Monster->MonsterCombatStats->IsDead()) { CancelSkill(); return; }

    Monster->HideAttackWarning();
    //캐릭터에 막히지 않고 지나가며 공격 판정은 별도의 Sweep으로 처리한다.
    Monster->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    Monster->GetCapsuleComponent()->SetCollisionResponseToChannel(MonsterCollision::Monster, ECR_Ignore);
    PlayChargeAnimation(ChargeRunAnimation);
    PreviousChargeLocation = Monster->GetActorLocation();
    bCharging = true;
    //이동 소스가 비정상적으로 진행되지 않을 때만 사용하는 종료 시각이다.
    ChargeEndTime = GetWorld()->GetTimeSeconds() + ChargeDistance / ChargeSpeed + 0.25f;

    //일정 방향의 이동을 CharacterMovement에 맡겨 프레임 속도와 무관하게 거리를 유지한다.
    TSharedPtr<FRootMotionSource_ConstantForce> Motion = MakeShared<FRootMotionSource_ConstantForce>();
    Motion->InstanceName = ChargeMotionName;
    Motion->AccumulateMode = ERootMotionAccumulateMode::Override;
    Motion->Priority = 500;
    Motion->Duration = ChargeDistance / ChargeSpeed;
    Motion->Force = ChargeDirection * ChargeSpeed;
    //마지막 프레임 전체를 이동하면 1200을 초과하므로 남은 시간만큼만 이동하도록 바꾼다.
    Motion->Settings.UnSetFlag(ERootMotionSourceSettingsFlags::DisablePartialEndTick);
    Motion->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::SetVelocity;
    Motion->FinishVelocityParams.SetVelocity = FVector::ZeroVector;
    Monster->GetCharacterMovement()->ApplyRootMotionSource(Motion);
    SetComponentTickEnabled(true);
}

void UMonsterSkill::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AMonsterBase* Monster = Cast<AMonsterBase>(GetOwner());
    if (!bCharging || !IsValid(Monster)) return;
    const FVector CurrentLocation = Monster->GetActorLocation();
    CheckChargeHits(PreviousChargeLocation, CurrentLocation);
    //피해 처리 중 가시 피해 등으로 시전자가 죽으면 이미 정리됐으므로 다시 진행하지 않는다.
    if (!bCharging || !IsValid(Monster)) return;
    const float Progress = FVector::DotProduct(CurrentLocation - PreviousChargeLocation, ChargeDirection);
    PreviousChargeLocation = CurrentLocation;
    const TSharedPtr<FRootMotionSource> Motion = Monster->GetCharacterMovement()->GetRootMotionSource(ChargeMotionName);
    if (GetWorld()->GetTimeSeconds() >= ChargeEndTime
        || !Motion || Motion->GetTime() >= ChargeDistance / ChargeSpeed
        || FVector::DotProduct(CurrentLocation - ChargeStart, ChargeDirection) >= ChargeDistance
        || (Motion->GetTime() > 0.0f && DeltaTime > 0.0f && Progress < ChargeSpeed * DeltaTime * 0.1f))
    {
        FinishSkill();
    }
}

void UMonsterSkill::CheckChargeHits(const FVector& Start, const FVector& End)
{
    AMonsterBase* Monster = Cast<AMonsterBase>(GetOwner());
    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    Objects.AddObjectTypesToQuery(MonsterCollision::Monster);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(MonsterCharge), false, Monster);
    TArray<FHitResult> Hits;
    const UCapsuleComponent* Capsule = Monster->GetCapsuleComponent();
    //점이 아니라 이동 구간 전체를 확인하므로 고속으로 지나간 대상도 한 번 적중한다.
    GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, Objects,
        FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query);
    for (const FHitResult& Hit : Hits)
    {
        AActor* Target = Hit.GetActor();
        if (!IsValid(Target) || ChargeHitActors.Contains(Target)) continue;
        ChargeHitActors.Add(Target);
        if (AMainPlayerCharacter* Player = Cast<AMainPlayerCharacter>(Target))
        {
            if (!Player->CombatStats || Player->CombatStats->IsDead()) continue;
            //플레이어는 캐릭터 이동 컴포넌트에 질량을 무시하는 순간 충격을 더한다.
            Player->GetCharacterMovement()->AddImpulse(ChargeDirection * 600.0f + FVector::UpVector * 300.0f, true);
            UGameplayStatics::ApplyDamage(Player, ChargeDamage, Monster->GetController(), Monster, nullptr);
            if (!bCharging || !IsValid(Monster)) return;
        }
        else if (AMonsterBase* OtherMonster = Cast<AMonsterBase>(Target))
        {
            //일반 근접·원거리만 피해 없이 날린다. 엘리트와 보스는 래그돌 대상이 아니다.
            if (!Cast<AEliteMonster>(OtherMonster) && !Cast<ABossMonster>(OtherMonster)
                && OtherMonster->GetAttackType() != EMonsterAttackType::Hybrid)
            {
                OtherMonster->BeginRagdoll(ChargeDirection * 600.0f);
            }
        }
    }
}

void UMonsterSkill::CleanupCharge()
{
    if (!bChargePrepared) return;
    bChargePrepared = false;
    bCharging = false;
    SetComponentTickEnabled(false);
    GetWorld()->GetTimerManager().ClearTimer(ChargeReadyTimer);
    if (AMonsterBase* Monster = Cast<AMonsterBase>(GetOwner()))
    {
        Monster->HideAttackWarning();
        UCharacterMovementComponent* Movement = Monster->GetCharacterMovement();
        Movement->RemoveRootMotionSource(ChargeMotionName);
        Movement->StopMovementImmediately();
        Movement->SetAvoidanceEnabled(bSavedAvoidance);
        Monster->GetCapsuleComponent()->SetCollisionResponseToChannels(SavedCapsuleResponses);
        PlayChargeAnimation(nullptr);
        //사망으로 정리되는 경우에는 BT를 다시 깨우지 않는다.
        if (!Monster->IsActorBeingDestroyed() && Monster->MonsterCombatStats && !Monster->MonsterCombatStats->IsDead())
        {
            if (AMonsterAIController* AI = Cast<AMonsterAIController>(Monster->GetController()))
            {
                AI->SetSkillMovementLocked(false);
            }
        }
    }
    ChargeHitActors.Reset();
}

void UMonsterSkill::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    //제거된 컴포넌트가 예약된 준비 동작이나 거리 검사를 실행하지 않도록 정리한다.
    GetWorld()->GetTimerManager().ClearTimer(ChargeCheckTimer);
    CleanupCharge();
    Super::EndPlay(EndPlayReason);
}

void UMonsterSkill::CancelSkill()
{
    if (!bIsUsingSkill) return;

    if (AMonsterBase* Monster = Cast<AMonsterBase>(GetOwner()))
    {
        Monster->HideAttackWarning();
    }
    FinishSkill();
}
