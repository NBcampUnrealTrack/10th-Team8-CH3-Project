#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

UENUM(BlueprintType)
enum class EWeaponSlot : uint8
{
	Pistol,
	Rifle
};

UCLASS(Abstract)
class DREAMVEIL_API AWeaponBase : public AActor
{
	GENERATED_BODY()
	
public:	

	AWeaponBase();
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void Fire(const FVector& MuzzleLocation, const FVector& FireDirection);

protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float Damage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float Range = 10000.0f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float FireInterval = 0.3f;

	//트레이스 선
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Debug")
	bool bDrawDebugTrace = true;

private:
	float LastFireTime = -100.0f;

	bool CanFire() const;
};
