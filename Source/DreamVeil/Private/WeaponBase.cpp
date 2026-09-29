#include "WeaponBase.h"
#include "AugmentDamageLibrary.h"
#include "AugmentTypes.h"
#include "CombatStatsComponent.h"
#include "DispatchTableComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "MonsterBase.h"


UWeaponBase::UWeaponBase()
{
	PrimaryComponentTick.bCanEverTick = false;

	//손에 든 무기가 캐릭터 이동이나 사격 트레이스를 막지 않도록 충돌을 끔
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);
	CanCharacterStepUpOn = ECB_No;
}

bool UWeaponBase::CanFire() const
{
	//파츠로 빨라진 발사 간격을 기준으로 잼
	return (GetWorld()->GetTimeSeconds() - LastFireTime) >= GetFireInterval();
}

FVector UWeaponBase::GetMuzzleLocation() const
{
	if (DoesSocketExist(MuzzleSocketName))
	{
		return GetSocketLocation(MuzzleSocketName);
	}

	return GetComponentLocation();
}

float UWeaponBase::GetRange() const
{
	return Range;
}

ECollisionChannel UWeaponBase::GetTraceChannel() const
{
	return TraceChannel;
}

bool UWeaponBase::IsAutomatic() const
{
	return bAutomatic;
}

float UWeaponBase::GetRecoilPitch() const
{
	return RecoilPitch;
}

float UWeaponBase::GetRecoilYaw() const
{
	return RecoilYaw;
}

//무기 데미지에 공격력을 더함 곱하면 공격력 증강으로 데미지가 너무 커져서 더하기로 함
//공격력 증가 광전사 같은 증강은 공격력 쪽에 반영되고 여기서 같이 들어감
float UWeaponBase::GetFinalDamage() const
{
	//파츠 공격력도 같은 이유로 곱하지 않고 더함
	float PartDamage = 0.0f;

	for (const FWeaponPart& Part : EquippedParts)
	{
		PartDamage += CalculatePartDamageBonus(Part);
	}

	return Damage + PartDamage + UAugmentDamageLibrary::GetOutgoingDamage(GetOwner());
}

void UWeaponBase::Fire(const FVector& MuzzleLocation, const FVector& FireDirection)
{
	if (!CanFire())
	{
		return;
	}
	LastFireTime = GetWorld()->GetTimeSeconds();

	//발사가 확정된 뒤에 재생 연사 간격에 막힌 호출에서 소리가 나면 안 되므로 CanFire 검사 아래에 둠
	PlayMuzzleEffects(MuzzleLocation);

	FVector TraceEnd = MuzzleLocation + (FireDirection * Range);

	//쏜 캐릭터 자신은 맞지 않음 무기는 컴포넌트라서 캐릭터를 무시하면 같이 무시됨
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());

	FHitResult Hit;
	bool bHitSomething = GetWorld()->LineTraceSingleByChannel(
		Hit, MuzzleLocation, TraceEnd, TraceChannel, QueryParams
	);

	if (bDrawDebugTrace)
	{
		FColor LineColor = bHitSomething ? FColor::Red : FColor::Green;
		DrawDebugLine(GetWorld(), MuzzleLocation, bHitSomething ? Hit.ImpactPoint : TraceEnd, LineColor, false, 1.0f, 0, 1.0f);
	}

	if (bHitSomething)
	{
		//데미지보다 먼저 재생 이번 발에 몬스터가 죽어서 사라져도 맞은 자리에 피는 튀게
		PlayImpactEffect(Hit);

		//쏜 캐릭터를 넘김 데미지 방어력 흡혈 가시 갑옷 적중 증강을 한 번에 처리
		//피해 적용으로 레그돌이 시작되기 전에 기존 머리 판정 함수를 재사용함
		const AMonsterBase* HitMonster = Cast<AMonsterBase>(Hit.GetActor());
		const bool bHeadshot = HitMonster && HitMonster->IsHeadshotHit(Hit);
		const float AppliedDamage = UAugmentDamageLibrary::ApplyWeaponHit(GetOwner(), Hit, GetFinalDamage());

		//실제로 데미지가 들어갔을 때만 히트 마커를 알림
		//벽과 바닥은 스탯 컴포넌트가 없어서 0이 돌아오고 아군을 맞혀도 0이라 마커가 안 뜸
		if (AppliedDamage > 0.0f)
		{
			//이번 발로 죽었는지는 데미지가 들어간 뒤에 물어봐야 맞음
			const UCombatStatsComponent* HitStats = Hit.GetActor()
				? Hit.GetActor()->FindComponentByClass<UCombatStatsComponent>() : nullptr;

			OnHitConfirmed.Broadcast(HitStats && HitStats->IsDead(), bHeadshot);
		}
	}
}

//총구 불꽃과 발사음을 재생
void UWeaponBase::PlayMuzzleEffects(const FVector& MuzzleLocation)
{
	if (MuzzleFlashEffect)
	{
		//총구 소켓에 붙여서 재생 소켓이 없으면 무기 원점에 붙음 GetMuzzleLocation과 같은 규칙
		//SnapToTarget은 소켓의 위치와 방향을 그대로 따름 그래서 소켓의 X축이 총구 앞을 봐야 불꽃이 앞으로 나감
		//bAutoDestroy가 true라 재생이 끝나면 알아서 지워짐 연사해도 컴포넌트가 쌓이지 않음
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			MuzzleFlashEffect, this, MuzzleSocketName,
			FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, true
		);
	}

	if (FireSound && FireSoundVolume > 0.0f)
	{
		//총소리는 짧아서 붙이지 않고 쏜 자리에서 한 번 재생
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, MuzzleLocation, FireSoundVolume);
	}
}

//맞은 곳에 이펙트를 재생
void UWeaponBase::PlayImpactEffect(const FHitResult& Hit)
{
	//맞은 게 폰이면 피 벽 바닥 물건이면 파편
	//몬스터 클래스가 아니라 폰으로 나누는 이유 나중에 몬스터가 총을 써서 플레이어가 맞아도 무기 코드를 안 고치고 피가 나게 하려고
	const bool bHitPawn = Cast<APawn>(Hit.GetActor()) != nullptr;

	//맞은 면의 바깥 방향(법선)을 이펙트의 앞(X축)으로 삼아서 표면 밖으로 튀게 함
	const FRotator ImpactRotation = Hit.ImpactNormal.Rotation();

	//맞은 대상에 따라 달라지는 연출(피 파편)은 폭발탄이 있어도 그대로 냄
	//맞았다는 표시까지 폭발에 묻히면 어디를 맞혔는지 안 보임
	SpawnImpactEffect(bHitPawn ? BloodEffect : ImpactEffect, Hit.ImpactPoint, ImpactRotation);

	//폭발탄을 가졌으면 추가 효과 자리에 폭발을 대신 냄
	//폭발 칸을 안 채웠으면 아래로 내려가서 원래 추가 효과가 그대로 나감
	if (ExplosionEffect && HasAreaAttackAugment())
	{
		SpawnExplosionEffect(Hit.ImpactPoint, ImpactRotation);

		return;
	}

	//어디를 맞았든 공통으로 나는 탄착 연출 피 파편과 겹쳐서 재생됨
	SpawnImpactEffect(bHitPawn ? BloodPointEffect : ImpactPointEffect, Hit.ImpactPoint, ImpactRotation);
}

//이펙트 하나를 탄착점에 재생
void UWeaponBase::SpawnImpactEffect(UNiagaraSystem* Effect, const FVector& Location, const FRotator& Rotation)
{
	//안 넣은 칸은 그냥 건너뜀 네 칸을 전부 채우지 않아도 되게 함
	if (!Effect)
	{
		return;
	}

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Effect, Location, Rotation);
}

//범위 피해로 맞은 자리에 피 연출을 냄
void UWeaponBase::PlaySplashHitEffect(const FVector& Location, const FVector& ExplosionCenter)
{
	//터진 중심에서 대상 쪽으로 향하는 방향 피가 폭발 바깥으로 튀어 보이게 함
	//같은 자리에서 터졌으면 방향이 없으므로 위쪽으로 둠
	const FVector OutwardDirection = (Location - ExplosionCenter).GetSafeNormal(KINDA_SMALL_NUMBER, FVector::UpVector);

	//범위 피해는 적에게만 들어가므로 언제나 피 쪽을 씀
	//추가 효과는 내지 않음 터지는 연출은 중심에서 한 번이면 충분하고 대상마다 내면 화면이 불꽃으로 덮임
	SpawnImpactEffect(BloodEffect, Location, OutwardDirection.Rotation());
}

//폭발을 탄착점에 재생
void UWeaponBase::SpawnExplosionEffect(const FVector& Location, const FRotator& Rotation)
{
	//이펙트의 원래 크기를 0으로 두면 나눌 수 없으므로 막음
	const float SafeBaseRadius = FMath::Max(ExplosionEffectBaseRadius, KINDA_SMALL_NUMBER);

	//피격 반경에 맞춰 키우거나 줄임 불덩이 크기와 데미지 범위가 항상 같아짐
	//반경만 고치면 이펙트가 따라오므로 밸런스를 바꿀 때 에셋을 다시 만들 필요가 없음
	const float EffectScale = AREA_ATTACK_RADIUS / SafeBaseRadius;

	UGameplayStatics::SpawnEmitterAtLocation(
		GetWorld(),
		ExplosionEffect,
		Location,
		Rotation,
		FVector(EffectScale),
		true
	);
}

//쏜 사람이 폭발탄 증강을 가졌는지
bool UWeaponBase::HasAreaAttackAugment() const
{
	//무기는 증강을 모름 증강 컴포넌트를 가진 쪽(플레이어나 보스)에게 물어봄
	const UDispatchTableComponent* OwnerTable = GetOwner() ? GetOwner()->FindComponentByClass<UDispatchTableComponent>() : nullptr;

	return OwnerTable && OwnerTable->HasAcquiredAugment(EAugmentID::AreaAttack);
}

//쏜 사람이 지금 들고 있는 무기
UWeaponBase* UWeaponBase::FindActiveWeapon(const AActor* Shooter)
{
	if (!Shooter)
	{
		return nullptr;
	}

	TArray<UWeaponBase*> Weapons;
	Shooter->GetComponents(Weapons);

	for (UWeaponBase* Weapon : Weapons)
	{
		//안 든 무기는 UpdateWeaponVisibility가 숨겨두므로 보이는 것이 들고 있는 것
		if (Weapon && !Weapon->bHiddenInGame)
		{
			return Weapon;
		}
	}

	return nullptr;
}

//이 총에 끼울 수 있는 칸
TArray<EWeaponPartSlot> UWeaponBase::GetPartSlots() const
{
	//모든 총이 같이 쓰는 공용 3칸 소총은 이걸 받아서 전용 칸을 더함
	return { EWeaponPartSlot::Muzzle, EWeaponPartSlot::Magazine, EWeaponPartSlot::Sight };
}

//끼운 파츠를 통째로 바꿈
void UWeaponBase::SetEquippedParts(const TArray<FWeaponPart>& NewParts)
{
	//수치는 쏠 때마다 이 목록에서 다시 계산하므로 여기서는 목록만 바꿔둠
	EquippedParts = NewParts;
}

//파츠까지 반영한 실제 발사 간격
float UWeaponBase::GetFireInterval() const
{
	float FireRateBonus = 0.0f;

	for (const FWeaponPart& Part : EquippedParts)
	{
		FireRateBonus += CalculatePartFireRateBonus(Part);
	}

	//연사력 파츠를 많이 끼워도 줄이는 비율에 상한을 둬서 발사 간격이 0에 가까워지지 않게 함
	return FireInterval * (1.0f - FMath::Min(FireRateBonus, MAX_PART_FIRE_RATE_BONUS));
}
