#include "WeaponBase.h"
#include "Kismet/KismetSystemLibrary.h"
#include "AugmentDamageLibrary.h"
#include "DrawDebugHelpers.h"


AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;
}

bool AWeaponBase::CanFire() const
{
	return (GetWorld()->GetTimeSeconds() - LastFireTime) >= FireInterval;
}

void AWeaponBase::Fire(const FVector& MuzzleLocation, const FVector& FireDirection)
{
	if (!CanFire())
	{
		return; 
	}
	LastFireTime = GetWorld()->GetTimeSeconds();

	FVector TraceEnd = MuzzleLocation + (FireDirection * Range);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(GetOwner());

	FHitResult Hit;
	bool bHitSomething = GetWorld()->LineTraceSingleByChannel(
		Hit, MuzzleLocation, TraceEnd, ECC_Visibility, QueryParams
	);

	if (bDrawDebugTrace)
	{
		FColor LineColor = bHitSomething ? FColor::Red : FColor::Green;
		DrawDebugLine(GetWorld(), MuzzleLocation, bHitSomething ? Hit.ImpactPoint : TraceEnd, LineColor, false, 1.0f, 0, 1.0f);
	}

	if (bHitSomething)
	{
		//쏜 캐릭터를 넘김 데미지 방어력 흡혈 가시 갑옷 적중 증강을 한 번에 처리
		UAugmentDamageLibrary::ApplyWeaponHit(GetOwner(), Hit, Damage);
	}
}

