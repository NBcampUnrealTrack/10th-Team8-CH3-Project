#include "BlackHoleZone.h"

#include "AugmentDamageLibrary.h"
#include "CombatStatsComponent.h"
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
	bZoneActive = false;
	Destroy();
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

void ABlackHoleZone::ApplyDamageTick()
{
	if (!bZoneActive || DamagePerTick <= 0.0f) return;

	TArray<AActor*> Targets;
	GatherTargets(Targets);

	for (AActor* Target : Targets)
	{
		//화염탄과 같은 표식 방어력 무시 흡혈 가시 갑옷 반사 없음
		UAugmentDamageLibrary::ApplyFireDamage(this, Target, DamagePerTick);
	}
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

	if (!bZoneActive || PullSpeed <= 0.0f) return;

	TArray<AActor*> Targets;
	GatherTargets(Targets);

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
