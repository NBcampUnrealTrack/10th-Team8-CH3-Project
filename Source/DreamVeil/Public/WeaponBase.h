#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "WeaponBase.generated.h"

UENUM(BlueprintType)
enum class EWeaponSlot : uint8
{
	Pistol,
	Rifle
};

//플레이어 손 소켓에 붙는 무기 컴포넌트 이 클래스 그대로가 기본 무기인 권총
//다른 무기는 이 클래스를 물려받아 수치만 바꿈 소총은 URifleWeapon
//캐릭터의 컴포넌트라서 GetOwner가 항상 쏜 캐릭터 흡혈과 가시 갑옷 반사가 그 캐릭터에게 감
//무기 모델은 블루프린트 Details의 Static Mesh에서 지정하고 붙일 소켓은 Parent Socket에서 바꿈
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DREAMVEIL_API UWeaponBase : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UWeaponBase();

	//MuzzleLocation에서 FireDirection으로 쏨 연사 간격이 안 지났으면 무시
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void Fire(const FVector& MuzzleLocation, const FVector& FireDirection);

	//총구 위치 무기 메시에 Muzzle 소켓이 있으면 그 위치 없으면 무기 컴포넌트 위치
	UFUNCTION(BlueprintPure, Category = "Weapon")
	FVector GetMuzzleLocation() const;

	//사거리 조준점 계산에 씀
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetRange() const;

	//실제로 들어갈 데미지 무기 데미지 + 쏜 캐릭터의 공격력 UI 표시용
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetFinalDamage() const;

protected:
	//기본값은 권총 수치 데미지는 높고 연사력은 낮음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float Damage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float Range = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float FireInterval = 0.35f;

	//사격 트레이스 채널 기본 Pawn 캐릭터가 막히는 곳에서 총알도 막힘
	//Visibility는 캐릭터 캡슐과 메시가 무시해서 총알이 몬스터를 통과하므로 쓰지 말 것
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Pawn;

	//총구 소켓 이름 무기 메시에 이 이름의 소켓을 만들면 거기서 발사
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	FName MuzzleSocketName = TEXT("Muzzle");

	//트레이스 선
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Debug")
	bool bDrawDebugTrace = true;

private:
	float LastFireTime = -100.0f;

	bool CanFire() const;
};
