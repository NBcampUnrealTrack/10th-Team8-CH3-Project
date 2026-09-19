#include "WeaponBase.h"
#include "AugmentDamageLibrary.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"


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
		UAugmentDamageLibrary::ApplyWeaponHit(GetOwner(), Hit, GetFinalDamage());
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

	if (FireSound)
	{
		//총소리는 짧아서 붙이지 않고 쏜 자리에서 한 번 재생
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, MuzzleLocation);
	}
}

//맞은 곳에 이펙트를 재생
void UWeaponBase::PlayImpactEffect(const FHitResult& Hit)
{
	//맞은 게 폰이면 피 벽 바닥 물건이면 파편
	//몬스터 클래스가 아니라 폰으로 나누는 이유 나중에 몬스터가 총을 써서 플레이어가 맞아도 무기 코드를 안 고치고 피가 나게 하려고
	UNiagaraSystem* EffectToPlay = Cast<APawn>(Hit.GetActor()) ? BloodEffect : ImpactEffect;

	if (!EffectToPlay)
	{
		return;
	}

	//맞은 면의 바깥 방향(법선)을 이펙트의 앞(X축)으로 삼아서 표면 밖으로 튀게 함
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, EffectToPlay, Hit.ImpactPoint, Hit.ImpactNormal.Rotation());
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
