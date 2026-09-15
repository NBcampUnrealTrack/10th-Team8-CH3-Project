#include "WeaponBase.h"
#include "AugmentDamageLibrary.h"
#include "DrawDebugHelpers.h"


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
	return (GetWorld()->GetTimeSeconds() - LastFireTime) >= FireInterval;
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

//무기 데미지에 공격력을 더함 곱하면 공격력 증강으로 데미지가 너무 커져서 더하기로 함
//공격력 증가 광전사 같은 증강은 공격력 쪽에 반영되고 여기서 같이 들어감
float UWeaponBase::GetFinalDamage() const
{
	return Damage + UAugmentDamageLibrary::GetOutgoingDamage(GetOwner());
}

void UWeaponBase::Fire(const FVector& MuzzleLocation, const FVector& FireDirection)
{
	if (!CanFire())
	{
		return;
	}
	LastFireTime = GetWorld()->GetTimeSeconds();

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
		//쏜 캐릭터를 넘김 데미지 방어력 흡혈 가시 갑옷 적중 증강을 한 번에 처리
		UAugmentDamageLibrary::ApplyWeaponHit(GetOwner(), Hit, GetFinalDamage());
	}
}
