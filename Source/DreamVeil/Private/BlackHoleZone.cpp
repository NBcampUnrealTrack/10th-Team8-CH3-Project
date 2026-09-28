#include "BlackHoleZone.h"

#include "AugmentDamageLibrary.h"
#include "AugmentTypes.h"
#include "CombatStatsComponent.h"
#include "DispatchTableComponent.h"
#include "Particles/ParticleSystem.h"
#include "MonsterBase.h"
#include "Components/AudioComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

ABlackHoleZone::ABlackHoleZone()
{
	//끌어당김과 이미지 회전에만 씀
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	ZoneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ZoneRoot"));
	SetRootComponent(ZoneRoot);

	//루트를 Yaw로 돌리면 바닥을 향한 데칼이 제자리에서 돌아감
	ZoneDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("ZoneDecal"));
	ZoneDecal->SetupAttachment(ZoneRoot);
	ZoneDecal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	ZoneDecal->SetFadeScreenSize(0.0f);

	//화염탄과 같은 틱 간격으로 시작 BP에서 바꿀 수 있음
	DamageInterval = CONTINUOUS_ATTACK_INTERVAL;
}

void ABlackHoleZone::SetDamageScale(float NewScale)
{
	DamageScale = FMath::Max(NewScale, 0.0f);
}

void ABlackHoleZone::SetFallbackWarningMaterial(UMaterialInterface* Material)
{
	FallbackWarningMaterial = Material;
}

void ABlackHoleZone::BeginPlay()
{
	Super::BeginPlay();

	//X는 투영 깊이, Y Z는 가로 세로 반지름 원형 이미지면 반지름 그대로 넣으면 됨
	ZoneDecal->DecalSize = FVector(DecalDepth, ZoneRadius, ZoneRadius);

	//경고 머티리얼 우선순위 BP 지정 -> 몬스터 경고 데칼 -> 블랙홀 머티리얼
	UMaterialInterface* ArmMaterial = WarningMaterial ? WarningMaterial.Get()
		: (FallbackWarningMaterial ? FallbackWarningMaterial.Get() : ZoneMaterial.Get());
	ApplyDecalMaterial(ArmMaterial);

	if (ArmDelay > 0.0f)
	{
		GetWorldTimerManager().SetTimer(ArmTimer, this, &ABlackHoleZone::ActivateZone, ArmDelay, false);
	}
	else
	{
		ActivateZone();
	}
}

void ABlackHoleZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ArmTimer);
	GetWorldTimerManager().ClearTimer(DamageTimer);
	GetWorldTimerManager().ClearTimer(DurationTimer);

	//어떤 이유로 사라지든 불꽃이 대상 몸에 남지 않게 끔 화염탄의 Deactivate와 같은 역할
	ExtinguishAll();

	if (IsValid(LoopAudio))
	{
		LoopAudio->Stop();
	}

	Super::EndPlay(EndPlayReason);
}

void ABlackHoleZone::ApplyDecalMaterial(UMaterialInterface* Material)
{
	if (!Material) return;
	ZoneDecal->SetDecalMaterial(Material);
	ZoneDecal->MarkRenderStateDirty();
}

void ABlackHoleZone::ActivateZone()
{
	bZoneActive = true;
	ApplyDecalMaterial(ZoneMaterial);

	if (ActivateSound && ActivateSoundVolume > 0.0f)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ActivateSound, GetActorLocation(), ActivateSoundVolume);
	}

	if (LoopSound && LoopSoundVolume > 0.0f)
	{
		//장판에 붙여서 재생 장판이 사라질 때 EndPlay에서 멈춤
		LoopAudio = UGameplayStatics::SpawnSoundAttached(
			LoopSound, ZoneRoot, NAME_None, FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset, true, LoopSoundVolume);
	}

	//첫 피해는 한 간격 뒤부터 화염탄과 같은 규칙
	GetWorldTimerManager().SetTimer(DamageTimer, this, &ABlackHoleZone::ApplyDamageTick, DamageInterval, true);
	GetWorldTimerManager().SetTimer(DurationTimer, this, &ABlackHoleZone::ExpireZone, Duration, false);
}

void ABlackHoleZone::ExpireZone()
{
	//장판은 끝났지만 나간 뒤에도 타는 대상이 남아 있으면 그 불이 다 꺼질 때까지 보이지 않게 남아 있음
	bZoneActive = false;
	bZoneExpired = true;
	ZoneDecal->SetVisibility(false);
	SetActorTickEnabled(false);
	if (IsValid(LoopAudio))
	{
		LoopAudio->Stop();
	}

	//남은 시간이 0인 대상은 장판이 끝나는 즉시 꺼짐
	TArray<TWeakObjectPtr<AActor>> Keys;
	BurningTargets.GetKeys(Keys);
	for (const TWeakObjectPtr<AActor>& WeakTarget : Keys)
	{
		const float* Remaining = BurningTargets.Find(WeakTarget);
		if (!WeakTarget.IsValid() || !Remaining || *Remaining <= UE_KINDA_SMALL_NUMBER)
		{
			ExtinguishTarget(WeakTarget.Get());
			BurningTargets.Remove(WeakTarget);
		}
	}

	DestroyIfDone();
}

void ABlackHoleZone::DestroyIfDone()
{
	if (bZoneExpired && BurningTargets.Num() == 0)
	{
		Destroy();
	}
}

UParticleSystem* ABlackHoleZone::GetOnFireEffect() const
{
	if (OnFireEffectOverride)
	{
		return OnFireEffectOverride;
	}

	//화염탄처럼 불꽃 에셋은 불을 붙인 쪽의 증강 컴포넌트가 들고 있음
	AActor* Caster = GetInstigator() ? static_cast<AActor*>(GetInstigator()) : GetOwner();
	const UDispatchTableComponent* CasterTable = Caster ? Caster->FindComponentByClass<UDispatchTableComponent>() : nullptr;
	return CasterTable ? CasterTable->OnFireEffect.Get() : nullptr;
}

void ABlackHoleZone::RefreshBurningTargets(const TArray<AActor*>& InZoneTargets)
{
	for (AActor* Target : InZoneTargets)
	{
		//이미 타고 있으면 남은 시간만 다시 채움
		BurningTargets.FindOrAdd(Target) = BurnLingerSeconds;

		//불붙은 상태는 대상 본인이 들고 있음 IsOnFire로 확인해서 이미 타고 있으면 건드리지 않음
		//다른 장판이 먼저 불을 꺼버린 경우에도 여기서 다시 붙음
		if (UCombatStatsComponent* TargetStats = Target->FindComponentByClass<UCombatStatsComponent>())
		{
			if (!TargetStats->IsOnFire())
			{
				TargetStats->SetOnFire(true, GetOnFireEffect());
			}
		}
	}
}

void ABlackHoleZone::ExtinguishTarget(AActor* Target)
{
	if (IsValid(Target))
	{
		if (UCombatStatsComponent* TargetStats = Target->FindComponentByClass<UCombatStatsComponent>())
		{
			TargetStats->SetOnFire(false, nullptr);
		}
		BurningTargets.Remove(Target);
	}
}

void ABlackHoleZone::ExtinguishAll()
{
	for (const TPair<TWeakObjectPtr<AActor>, float>& BurnPair : BurningTargets)
	{
		//이미 사라진 대상은 Get이 nullptr이라 건너뜀 그때는 불꽃도 같이 사라져 있음
		if (AActor* BurningActor = BurnPair.Key.Get())
		{
			if (UCombatStatsComponent* BurningStats = BurningActor->FindComponentByClass<UCombatStatsComponent>())
			{
				BurningStats->SetOnFire(false, nullptr);
			}
		}
	}
	BurningTargets.Empty();
}

bool ABlackHoleZone::IsHostile(AActor* Target) const
{
	if (!IsValid(Target) || Target == this) return false;

	//시전한 몬스터 기준으로 판정 몬스터끼리는 아군이라 다른 몬스터는 안 맞음
	AActor* Caster = GetInstigator() ? static_cast<AActor*>(GetInstigator()) : GetOwner();
	if (Caster) return UAugmentDamageLibrary::IsEnemy(Caster, Target);
	return !Target->IsA<AMonsterBase>();
}

void ABlackHoleZone::GatherTargets(TArray<AActor*>& OutTargets) const
{
	OutTargets.Reset();

	const FVector Center = GetActorLocation();

	//캐릭터 위치는 캡슐 중심이라 바닥보다 높음 높이까지 포함하는 구로 넉넉히 찾고 아래에서 원통으로 거름
	const float SearchRadius = FMath::Sqrt(FMath::Square(ZoneRadius) + FMath::Square(ZoneHeight));

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(const_cast<ABlackHoleZone*>(this));
	if (AActor* Caster = GetInstigator()) ActorsToIgnore.Add(Caster);

	TArray<AActor*> Found;
	UAugmentDamageLibrary::FindTargetsAtLocation(const_cast<ABlackHoleZone*>(this), Center, SearchRadius, ActorsToIgnore, Found);

	for (AActor* Target : Found)
	{
		if (!IsHostile(Target)) continue;

		const UCombatStatsComponent* Stats = Target->FindComponentByClass<UCombatStatsComponent>();
		if (!Stats || Stats->IsDead()) continue;

		const FVector Offset = Target->GetActorLocation() - Center;
		if (Offset.SizeSquared2D() > FMath::Square(ZoneRadius)) continue;
		if (FMath::Abs(Offset.Z) > ZoneHeight) continue;

		OutTargets.Add(Target);
	}
}

//UContinuousAttackSkill::ProcessBurnTick과 같은 규칙으로 한 틱 처리
void ABlackHoleZone::ApplyDamageTick()
{
	//장판 안에 있는 대상 갱신 장판이 끝난 뒤에는 새로 붙이지 않고 남은 불만 태움
	TArray<AActor*> InZoneTargets;
	if (bZoneActive)
	{
		GatherTargets(InZoneTargets);
		RefreshBurningTargets(InZoneTargets);
	}

	//데미지 처리 중에 목록이 바뀔 수 있어서 키를 복사해서 돌림
	TArray<TWeakObjectPtr<AActor>> Keys;
	BurningTargets.GetKeys(Keys);

	for (const TWeakObjectPtr<AActor>& WeakTarget : Keys)
	{
		AActor* Target = WeakTarget.Get();
		float* Remaining = BurningTargets.Find(WeakTarget);
		if (!Target || !Remaining)
		{
			BurningTargets.Remove(WeakTarget);
			continue;
		}

		//장판 밖이면 남은 시간을 깎고 다 떨어졌으면 불을 끔
		if (!InZoneTargets.Contains(Target))
		{
			*Remaining -= DamageInterval;
			if (*Remaining < -UE_KINDA_SMALL_NUMBER)
			{
				ExtinguishTarget(Target);
				continue;
			}
		}

		if (DamagePerTick <= 0.0f)
		{
			continue;
		}

		//화염탄과 같은 표식 방어력 무시 흡혈 가시 갑옷 반사 없음
		//대상이 이미 죽어서 데미지가 안 들어가면 0이 돌아오고 그때 불을 끔
		//DamagePerTick은 쉬움 L1 기준 시전 몬스터의 스킬 피해 배율을 곱해서 줌
		const float AppliedDamage = UAugmentDamageLibrary::ApplyFireDamage(this, Target, DamagePerTick * DamageScale);
		if (AppliedDamage <= 0.0f)
		{
			ExtinguishTarget(Target);
		}
	}

	DestroyIfDone();
}

void ABlackHoleZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!FMath::IsNearlyZero(SpinSpeed))
	{
		ZoneRoot->AddLocalRotation(FRotator(0.0f, SpinSpeed * DeltaSeconds, 0.0f));
	}

	if (bDrawDebug)
	{
		const FVector Center = GetActorLocation();
		DrawDebugCylinder(GetWorld(), Center - FVector(0, 0, ZoneHeight), Center + FVector(0, 0, ZoneHeight),
			ZoneRadius, 32, bZoneActive ? FColor::Green : FColor::Yellow, false, -1.0f);
	}

	if (!bZoneActive) return;

	TArray<AActor*> Targets;
	GatherTargets(Targets);

	//들어오자마자 불이 붙어 보이게 함 데미지는 타이머 간격대로만 들어감
	RefreshBurningTargets(Targets);

	if (PullSpeed <= 0.0f) return;

	const FVector Center = GetActorLocation();
	for (AActor* Target : Targets)
	{
		ACharacter* Character = Cast<ACharacter>(Target);
		if (!Character) continue;

		FVector ToCenter = Center - Character->GetActorLocation();
		ToCenter.Z = 0.0f;
		const float Distance = ToCenter.Size();
		if (Distance <= PullDeadZone) continue;

		//힘(AddImpulse)이 아니라 위치를 직접 옮기는 이유
		//가만히 서 있으면 걷기 제동이 작은 힘을 전부 지워버려서 끌려가지 않음
		//스윕으로 옮겨서 벽을 뚫지 않고, 한가운데를 넘어가지 않게 남은 거리까지만 옮김
		const float Step = FMath::Min(PullSpeed * DeltaSeconds, Distance - PullDeadZone);
		Character->AddActorWorldOffset(ToCenter / Distance * Step, true);
	}
}
