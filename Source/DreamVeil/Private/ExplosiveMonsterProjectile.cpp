#include "ExplosiveMonsterProjectile.h"

#include "AugmentDamageLibrary.h"
#include "AugmentTypes.h"
#include "CombatStatsComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "MonsterBase.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"

AExplosiveMonsterProjectile::AExplosiveMonsterProjectile()
{
	//플레이어 폭발탄과 같은 값으로 시작 블루프린트에서 몬스터마다 바꿀 수 있음
	ExplosionRadius = AREA_ATTACK_RADIUS;
	SplashDamageRatio = AREA_ATTACK_DAMAGE_RATIO;
	SplashMinFalloff = AREA_ATTACK_MIN_FALLOFF;
	bDrawExplosionDebug = AREA_ATTACK_DRAW_DEBUG;
}

void AExplosiveMonsterProjectile::ProcessHit(AActor* OtherActor, const FHitResult& Hit)
{
	//투사체는 스폰할 때 Instigator로 쏜 몬스터를 받음 없으면 Owner로 대신함
	AActor* Shooter = GetInstigator() ? static_cast<AActor*>(GetInstigator()) : GetOwner();

	//벽이나 바닥에 맞았어도 그 자리에서 터짐 충돌 지점이 비어 있으면 투사체 위치를 씀
	const FVector Center = Hit.bBlockingHit ? FVector(Hit.ImpactPoint) : GetActorLocation();
	const FRotator EffectRotation = Hit.bBlockingHit ? Hit.ImpactNormal.Rotation() : GetActorRotation();

	//데미지보다 먼저 재생 이번 폭발로 대상이 죽어서 사라져도 연출은 남게
	SpawnExplosionEffect(Center, EffectRotation);
	PlayExplosionSound(Center);

	//직격 대상은 폭발탄과 같이 온전한 피해를 받음
	AActor* DirectHitActor = nullptr;
	if (IsValid(OtherActor)
		&& OtherActor->FindComponentByClass<UCombatStatsComponent>()
		&& IsHostileTarget(Shooter, OtherActor))
	{
		DirectHitActor = OtherActor;
		UAugmentDamageLibrary::ApplyAugmentDamageToTarget(this, OtherActor, GetDamage());
	}

	ApplySplashDamage(Shooter, Center, DirectHitActor);
}

bool AExplosiveMonsterProjectile::IsHostileTarget(AActor* Shooter, AActor* Target) const
{
	if (!IsValid(Target) || Target == this) return false;
	if (Shooter) return UAugmentDamageLibrary::IsEnemy(Shooter, Target);
	return !Target->IsA<AMonsterBase>();
}

//UAreaAttackSkill::OnWeaponHit와 같은 규칙
void AExplosiveMonsterProjectile::ApplySplashDamage(AActor* Shooter, const FVector& Center, AActor* DirectHitActor)
{
	const float SplashDamage = GetDamage() * SplashDamageRatio;
	if (SplashDamage <= 0.0f || ExplosionRadius <= 0.0f) return;

	//쏜 몬스터와 투사체 자신 직격 대상은 제외 직격 대상은 이미 온전한 피해를 받음
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(this);
	if (Shooter) ActorsToIgnore.Add(Shooter);
	if (DirectHitActor) ActorsToIgnore.Add(DirectHitActor);

	TArray<AActor*> FoundTargets;
	UAugmentDamageLibrary::FindTargetsAtLocation(this, Center, ExplosionRadius, ActorsToIgnore, FoundTargets);

	//몬스터끼리는 아군 주변 몬스터가 휘말리지 않도록 적만 남김
	FoundTargets.RemoveAll([this, Shooter](AActor* FoundTarget)
		{
			return !IsHostileTarget(Shooter, FoundTarget);
		});

	if (bDrawExplosionDebug)
	{
		DrawDebugSphere(GetWorld(), Center, ExplosionRadius, 16, FColor::Red, false, 0.4f);
		UE_LOG(LogTemp, Warning, TEXT("[MonsterExplosion] 반경 %.0f 안에서 %d명 적중 최대 데미지 %.1f"),
			ExplosionRadius, FoundTargets.Num(), SplashDamage);
	}

	for (AActor* FoundTarget : FoundTargets)
	{
		//터진 자리에서 멀수록 약하게 맞음 가장자리에도 최소 비율은 남김
		const float DistanceToCenter = FVector::Dist(FoundTarget->GetActorLocation(), Center);
		const float CenterRatio = 1.0f - FMath::Clamp(DistanceToCenter / ExplosionRadius, 0.0f, 1.0f);
		const float FalloffRatio = FMath::Lerp(SplashMinFalloff, 1.0f, CenterRatio);

		UAugmentDamageLibrary::ApplyAugmentDamageToTarget(this, FoundTarget, SplashDamage * FalloffRatio);
	}
}

//UWeaponBase::SpawnExplosionEffect와 같은 방식 반경에 맞춰 이펙트를 키우거나 줄임
void AExplosiveMonsterProjectile::SpawnExplosionEffect(const FVector& Location, const FRotator& Rotation) const
{
	if (!ExplosionEffect) return;

	const float SafeBaseRadius = FMath::Max(ExplosionEffectBaseRadius, UE_KINDA_SMALL_NUMBER);
	const float EffectScale = ExplosionRadius / SafeBaseRadius;

	UGameplayStatics::SpawnEmitterAtLocation(
		GetWorld(),
		ExplosionEffect,
		Location,
		Rotation,
		FVector(EffectScale),
		true
	);
}

//폭발 소리를 터진 자리에서 재생 투사체는 곧바로 파괴되므로 붙이지 않고 월드 위치에 재생함
void AExplosiveMonsterProjectile::PlayExplosionSound(const FVector& Location) const
{
	if (!ExplosionSound || ExplosionSoundVolume <= 0.0f) return;

	UGameplayStatics::PlaySoundAtLocation(
		this,
		ExplosionSound,
		Location,
		FRotator::ZeroRotator,
		ExplosionSoundVolume,
		ExplosionSoundPitch,
		0.0f,
		ExplosionSoundAttenuation
	);
}
