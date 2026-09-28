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
#include "Components/DecalComponent.h"
#include "MonsterProjectile.h"
#include "BlackHoleZone.h"

namespace
{
    //돌진 거리 속도 준비 시간 폭은 헤더의 UPROPERTY로 옮겨서 BP에서 조절한다.
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
        GetWorld()->GetTimerManager().SetTimer(ChargeCheckTimer, this, &UMonsterSkill::CheckSkillRange, 0.2f, true);
    }
}

void UMonsterSkill::CheckSkillRange()
{
    //Skills에 등록한 스킬 중 지금 쓸 수 있는 것만 모은다. 쿨타임 중에는 AI 행동을 건드리지 않는다.
    TArray<EMonsterSkillType> ReadySkills;
    for (const FMonsterSkillSettings& Settings : Skills)
    {
        if (CanUseSkill(Settings.Skill)) ReadySkills.AddUnique(Settings.Skill);
    }

    //항상 같은 순서로 시도하면 첫 스킬만 나오므로 섞어서 시도하고, 거리 조건이 맞는 첫 스킬을 쓴다.
    for (int32 i = ReadySkills.Num() - 1; i > 0; --i)
    {
        ReadySkills.Swap(i, FMath::RandRange(0, i));
    }
    for (const EMonsterSkillType Skill : ReadySkills)
    {
        if (TryUseSkill(Skill)) return;
    }
}

const FMonsterSkillSettings* UMonsterSkill::FindSettings(EMonsterSkillType Skill) const
{
    return Skills.FindByPredicate([Skill](const FMonsterSkillSettings& Settings)
    {
        return Settings.Skill == Skill;
    });
}

void UMonsterSkill::SetProgression(int32 Phase, int32 Level, bool bCheckPhase)
{
    //페이즈 값을 억지로 최대로 올리지 않고, 해당 조건을 사용할지 별도로 저장함
    bCheckRequiredPhase = bCheckPhase;
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
        //맵 진행 방식에서는 체력 페이즈와 관계없이 해당 레벨에 열린 스킬을 사용할 수 있음
        && (!bCheckRequiredPhase || CurrentPhase >= Settings->RequiredPhase)
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
    CleanupFanShot();
    CleanupBlackHole();
}

bool UMonsterSkill::TryUseSkill(EMonsterSkillType Skill)
{
    //스킬 종류별 실제 패턴으로 연결한다. 새 스킬은 여기에 분기를 추가한다.
    switch (Skill)
    {
    case EMonsterSkillType::Charge:  return TryUseCharge();
    case EMonsterSkillType::FanShot: return TryUseFanShot();
    case EMonsterSkillType::BlackHole: return TryUseBlackHole();
    default:                         return false;
    }
}

bool UMonsterSkill::TryUseCharge()
{
    const EMonsterSkillType Skill = EMonsterSkillType::Charge;
    AMonsterBase* Monster = Cast<AMonsterBase>(GetOwner());
    AMainPlayerCharacter* Player = Cast<AMainPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));

	//스킬 이중체크, 쿨돌았고 플레이어 감지 제대로 됐는지 확인...
    if (!CanUseSkill(Skill) || !IsValid(Player)) return false;
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
        ChargeWidth);
    PlayChargeAnimation(ChargeReadyAnimation);
    //준비 시간이 0이어도 타이머가 걸리도록 최소값을 둔다.
    GetWorld()->GetTimerManager().SetTimer(ChargeReadyTimer, this, &UMonsterSkill::StartCharge, FMath::Max(ChargeReadySeconds, 0.01f), false);
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
    //설정값은 전체 폭이고 캡슐 검사는 반지름을 받으므로 절반으로 변환한다.
    const float AttackRadius = ChargeWidth * 0.5f;
    //캡슐의 반높이는 반지름보다 작을 수 없어 폭을 크게 설정한 경우에만 높이도 보정한다.
    const float AttackHalfHeight = FMath::Max(AttackRadius, Capsule->GetScaledCapsuleHalfHeight());
    //점이 아니라 이동 구간 전체를 확인하므로 고속으로 지나간 대상도 한 번 적중한다.
    GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, Objects,
        FCollisionShape::MakeCapsule(AttackRadius, AttackHalfHeight), Query);
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
            //쉬움 L1 기준 피해에 몬스터의 스킬 피해 배율(난이도 레벨 증강)을 곱한다. 평타와 같은 비율로 강해진다.
            UGameplayStatics::ApplyDamage(Player, ChargeDamage * Monster->GetSkillDamageScale(), Monster->GetController(), Monster, nullptr);
            if (!bCharging || !IsValid(Monster)) return;
        }
        else if (AMonsterBase* OtherMonster = Cast<AMonsterBase>(Target))
        {
            //일반 근접·원거리만 피해 없이 날린다. 엘리트와 보스는 래그돌 대상이 아니다.
            if (!Cast<AEliteMonster>(OtherMonster) && !Cast<ABossMonster>(OtherMonster)
                && OtherMonster->GetAttackType() != EMonsterAttackType::Hybrid)
            {
                OtherMonster->BeginRagdoll(ChargeDirection * 1000.0f);
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
    CleanupFanShot();
    CleanupBlackHole();
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

// ======================== 부채꼴 투사체 (FanShot) ========================

TArray<FVector> UMonsterSkill::GetFanShotDirections() const
{
    //부채꼴 전체 각도를 발사 수에 맞게 나눈다. 한 발이면 정면으로만 쏜다.
    TArray<FVector> Directions;
    const int32 Count = FMath::Max(1, FanShotCount);
    //360도면 처음과 끝이 겹치므로 간격 수를 한 칸 늘린다.
    const bool bFullCircle = FanShotAngle >= 360.0f - UE_KINDA_SMALL_NUMBER;
    const float Step = Count > 1 ? FanShotAngle / static_cast<float>(bFullCircle ? Count : Count - 1) : 0.0f;
    const float StartYaw = Count > 1 && !bFullCircle ? -FanShotAngle * 0.5f : 0.0f;
    for (int32 i = 0; i < Count; ++i)
    {
        Directions.Add(FanShotDirection.RotateAngleAxis(StartYaw + Step * i, FVector::UpVector));
    }
    return Directions;
}

bool UMonsterSkill::TryUseFanShot()
{
    const EMonsterSkillType Skill = EMonsterSkillType::FanShot;
    AMonsterBase* Monster = Cast<AMonsterBase>(GetOwner());
    AMainPlayerCharacter* Player = Cast<AMainPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));

    if (!CanUseSkill(Skill) || !IsValid(Player) || !IsValid(Monster)) return false;
    if (!Player->CombatStats || Player->CombatStats->IsDead()) return false;
    //쏠 투사체가 없으면 발동하지 않는다.
    if (!FanShotProjectile && !Monster->GetRangedProjectileClass()) return false;

    const FVector ToPlayer = Player->GetActorLocation() - Monster->GetActorLocation();
    if (ToPlayer.SizeSquared() > FMath::Square(FanShotTriggerDistance) || ToPlayer.SizeSquared2D() <= UE_SMALL_NUMBER) return false;
    if (!BeginSkill(Skill)) return false;

    //방향은 준비 시작 시점에 고정한다. 경고를 보고 옆으로 피할 수 있게 하려는 것.
    FanShotDirection = ToPlayer.GetSafeNormal2D();
    bFanShotPrepared = true;

    if (AMonsterAIController* AI = Cast<AMonsterAIController>(Monster->GetController()))
    {
        AI->SetSkillMovementLocked(true);
    }
    Monster->GetCharacterMovement()->StopMovementImmediately();
    Monster->SetActorRotation(FanShotDirection.Rotation());

    //데칼은 발밑 높이에서 투영한다.
    const FVector Origin = Monster->GetActorLocation()
        - FVector::UpVector * Monster->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    ShowFanShotWarning(Origin);
    PlayChargeAnimation(FanShotReadyAnimation);

    GetWorld()->GetTimerManager().SetTimer(FanShotReadyTimer, this, &UMonsterSkill::FireFanShot,
        FMath::Max(FanShotReadySeconds, 0.01f), false);
    return true;
}

void UMonsterSkill::FireFanShot()
{
    AMonsterBase* Monster = Cast<AMonsterBase>(GetOwner());
    if (!bIsUsingSkill || !bFanShotPrepared || !IsValid(Monster)) return;
    if (!Monster->MonsterCombatStats || Monster->MonsterCombatStats->IsDead()) { CancelSkill(); return; }

    HideFanShotWarning();
    PlayChargeAnimation(nullptr);

    const TSubclassOf<AMonsterProjectile> ProjectileClass = FanShotProjectile ? FanShotProjectile : Monster->GetRangedProjectileClass();
    if (ProjectileClass)
    {
        //몬스터의 기존 발사 위치 오프셋을 발사 방향 기준으로 적용한다.
        const FRotator BaseRotation = FanShotDirection.Rotation();
        const FVector SpawnLocation = Monster->GetActorLocation() + BaseRotation.RotateVector(Monster->GetProjectileSpawnOffset());

        TArray<AMonsterProjectile*> Spawned;
        for (const FVector& Direction : GetFanShotDirections())
        {
            const FTransform SpawnTransform(Direction.Rotation(), SpawnLocation);
            AMonsterProjectile* Projectile = GetWorld()->SpawnActorDeferred<AMonsterProjectile>(
                ProjectileClass, SpawnTransform, Monster, Monster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if (!Projectile) continue;

            //쉬움 L1 기준 피해에 스킬 피해 배율을 곱한다. 폭발탄이면 폭발 피해도 이 값을 기준으로 계산된다.
            Projectile->SetDamage(FanShotDamage * Monster->GetSkillDamageScale());
            UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
            Spawned.Add(Projectile);
        }

        //같은 위치에서 동시에 나가므로 서로 부딪혀 사라지지 않게 서로를 무시시킨다.
        for (AMonsterProjectile* A : Spawned)
        {
            for (AMonsterProjectile* B : Spawned)
            {
                if (A != B) A->IgnoreActorWhileMoving(B);
            }
        }
    }

    //발사 직후 잠깐 멈춰 있다가 스킬을 끝낸다. 쿨타임은 FinishSkill에서 시작된다.
    if (FanShotRecoverySeconds > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(FanShotRecoveryTimer, this, &UMonsterSkill::FinishSkill,
            FanShotRecoverySeconds, false);
    }
    else
    {
        FinishSkill();
    }
}

void UMonsterSkill::ShowFanShotWarning(const FVector& Origin)
{
    HideFanShotWarning();

    AMonsterBase* Monster = Cast<AMonsterBase>(GetOwner());
    UMaterialInterface* WarningMaterial = Monster ? Monster->GetAttackWarningMaterial() : nullptr;
    if (!WarningMaterial || FanShotWarningLength <= 0.0f || FanShotWarningWidth <= 0.0f) return;

    //몬스터의 기존 경고 데칼과 같은 머티리얼로 투사체 경로마다 한 줄씩 그린다.
    for (const FVector& Direction : GetFanShotDirections())
    {
        UDecalComponent* Decal = NewObject<UDecalComponent>(Monster);
        if (!Decal) continue;
        Decal->SetDecalMaterial(WarningMaterial);
        Decal->SetFadeScreenSize(0.0f);
        Decal->RegisterComponent();

        //MonsterBase::ShowAttackWarning과 같은 방식: 바닥을 향하게 -90도, 길이 방향으로 Yaw 회전
        const FVector Center = Origin + Direction * (FanShotWarningLength * 0.5f);
        Decal->SetWorldLocationAndRotation(Center, FRotator(-90.0f, Direction.Rotation().Yaw, 0.0f));
        Decal->SetWorldScale3D(FVector::OneVector);
        Decal->DecalSize = FVector(100.0f, FanShotWarningWidth * 0.5f, FanShotWarningLength * 0.5f);
        Decal->MarkRenderStateDirty();

        FanShotWarningDecals.Add(Decal);
    }
}

void UMonsterSkill::HideFanShotWarning()
{
    for (UDecalComponent* Decal : FanShotWarningDecals)
    {
        if (IsValid(Decal)) Decal->DestroyComponent();
    }
    FanShotWarningDecals.Reset();
}

void UMonsterSkill::CleanupFanShot()
{
    if (!bFanShotPrepared) return;
    bFanShotPrepared = false;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(FanShotReadyTimer);
        World->GetTimerManager().ClearTimer(FanShotRecoveryTimer);
    }
    HideFanShotWarning();
    PlayChargeAnimation(nullptr);

    if (AMonsterBase* Monster = Cast<AMonsterBase>(GetOwner()))
    {
        //사망으로 정리되는 경우에는 BT를 다시 깨우지 않는다.
        if (!Monster->IsActorBeingDestroyed() && Monster->MonsterCombatStats && !Monster->MonsterCombatStats->IsDead())
        {
            if (AMonsterAIController* AI = Cast<AMonsterAIController>(Monster->GetController()))
            {
                AI->SetSkillMovementLocked(false);
            }
        }
    }
}

// ======================== 블랙홀 장판 (BlackHole) ========================

bool UMonsterSkill::TryUseBlackHole()
{
    const EMonsterSkillType Skill = EMonsterSkillType::BlackHole;
    AMonsterBase* Monster = Cast<AMonsterBase>(GetOwner());
    AMainPlayerCharacter* Player = Cast<AMainPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));

    if (!CanUseSkill(Skill) || !IsValid(Player) || !IsValid(Monster)) return false;
    if (!Player->CombatStats || Player->CombatStats->IsDead()) return false;

    const FVector ToPlayer = Player->GetActorLocation() - Monster->GetActorLocation();
    if (ToPlayer.SizeSquared() > FMath::Square(BlackHoleTriggerDistance)) return false;

    //장판은 플레이어 발밑 바닥에 깐다. 공중에 떠 있어도 바로 아래 바닥을 찾는다.
    const float PlayerHalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FVector ZoneLocation = Player->GetActorLocation() - FVector(0.0f, 0.0f, PlayerHalfHeight);
    FHitResult FloorHit;
    FCollisionQueryParams FloorParams(SCENE_QUERY_STAT(BlackHoleFloor), false, Player);
    FloorParams.AddIgnoredActor(Monster);
    if (GetWorld()->LineTraceSingleByObjectType(FloorHit, Player->GetActorLocation(),
        Player->GetActorLocation() - FVector(0.0f, 0.0f, PlayerHalfHeight + 1000.0f),
        FCollisionObjectQueryParams(ECC_WorldStatic), FloorParams))
    {
        ZoneLocation = FloorHit.ImpactPoint;
    }

    if (!BeginSkill(Skill)) return false;
    bBlackHolePrepared = true;

    //시전 동안 멈춰서 플레이어 쪽을 바라본다.
    if (AMonsterAIController* AI = Cast<AMonsterAIController>(Monster->GetController()))
    {
        AI->SetSkillMovementLocked(true);
    }
    Monster->GetCharacterMovement()->StopMovementImmediately();
    if (ToPlayer.SizeSquared2D() > UE_SMALL_NUMBER)
    {
        Monster->SetActorRotation(ToPlayer.GetSafeNormal2D().Rotation());
    }
    PlayChargeAnimation(BlackHoleCastAnimation);

    //장판은 스스로 경고 -> 발동 -> 소멸을 처리한다. 시전자가 죽어도 이미 깔린 장판은 남는다.
    const TSubclassOf<ABlackHoleZone> ZoneClass = BlackHoleZoneClass ? BlackHoleZoneClass : TSubclassOf<ABlackHoleZone>(ABlackHoleZone::StaticClass());
    const FTransform ZoneTransform(FRotator::ZeroRotator, ZoneLocation);
    if (ABlackHoleZone* Zone = GetWorld()->SpawnActorDeferred<ABlackHoleZone>(
        ZoneClass, ZoneTransform, Monster, Monster, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
    {
        //장판 BP에 경고 머티리얼이 없으면 몬스터의 경고 데칼 머티리얼을 쓴다.
        Zone->SetFallbackWarningMaterial(Monster->GetAttackWarningMaterial());
        //장판 BP의 DamagePerTick은 쉬움 L1 기준 깔리는 순간의 배율로 고정한다.
        Zone->SetDamageScale(Monster->GetSkillDamageScale());
        UGameplayStatics::FinishSpawningActor(Zone, ZoneTransform);
    }

    if (BlackHoleCastSeconds > 0.0f)
    {
        GetWorld()->GetTimerManager().SetTimer(BlackHoleCastTimer, this, &UMonsterSkill::FinishSkill,
            BlackHoleCastSeconds, false);
    }
    else
    {
        FinishSkill();
    }
    return true;
}

void UMonsterSkill::CleanupBlackHole()
{
    if (!bBlackHolePrepared) return;
    bBlackHolePrepared = false;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(BlackHoleCastTimer);
    }
    PlayChargeAnimation(nullptr);

    if (AMonsterBase* Monster = Cast<AMonsterBase>(GetOwner()))
    {
        //사망으로 정리되는 경우에는 BT를 다시 깨우지 않는다.
        if (!Monster->IsActorBeingDestroyed() && Monster->MonsterCombatStats && !Monster->MonsterCombatStats->IsDead())
        {
            if (AMonsterAIController* AI = Cast<AMonsterAIController>(Monster->GetController()))
            {
                AI->SetSkillMovementLocked(false);
            }
        }
    }
}
