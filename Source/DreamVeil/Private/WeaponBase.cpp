#include "WeaponBase.h"
#include "HealthComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DrawDebugHelpers.h"


AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AWeaponBase::BeginPlay()
{

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
		if (AActor* HitActor = Hit.GetActor())
		{
			if (UHealthComponent* TargetHealth = HitActor->FindComponentByClass<UHealthComponent>())
			{
				TargetHealth->ApplyDamage(Damage);
			}
		}
	}
}

