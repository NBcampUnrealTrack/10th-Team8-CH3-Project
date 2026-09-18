#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "WeaponBase.generated.h"

class UNiagaraSystem;
class USoundBase;

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

	//사격 트레이스 채널 조준점을 찾을 때도 같은 채널을 써야 결과가 맞음
	ECollisionChannel GetTraceChannel() const;

	//마지막 발사 뒤로 FireInterval이 지났는지 쏘는 쪽에서 조준 계산 전에 미리 확인할 때 씀
	//Fire 안에서도 다시 확인하므로 이걸 안 불러도 연사 간격은 지켜짐
	bool CanFire() const;

	//누르고 있으면 계속 쏘는 무기인지 false면 누를 때마다 한 발
	//무기는 값만 알려주고 실제로 거르는 건 쏘는 쪽 입력 함수 플레이어는 AMainPlayerCharacter::FireWeaponHeld
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsAutomatic() const;

protected:
	//기본값은 권총 수치 데미지는 높고 연사력은 낮음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float Damage = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float Range = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	float FireInterval = 0.35f;

	//연사 무기인지 true면 누르고 있는 동안 FireInterval마다 계속 발사 기본 권총은 단발 소총은 URifleWeapon에서 true
	//무기 안에서는 이 값으로 발사를 막지 않음 무기는 버튼을 누른 순간인지 누르고 있는 중인지 모르기 때문
	//그래서 무기는 값만 들고 있고 입력을 받는 쪽이 IsAutomatic으로 보고 쏠지 말지 정함
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	bool bAutomatic = false;

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

	//총구 불꽃 이펙트 쏠 때마다 총구 소켓에 붙여서 재생 비워두면 불꽃 없이 쏨
	//소켓에 붙이는 이유 연사 중에 캐릭터가 움직여도 불꽃이 총구를 따라가게 하려고
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Effect")
	TObjectPtr<UNiagaraSystem> MuzzleFlashEffect;

	//발사음 총구 위치에서 재생 비워두면 소리 없이 쏨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Effect")
	TObjectPtr<USoundBase> FireSound;

	//벽 바닥 물건처럼 폰이 아닌 곳에 맞았을 때 튀는 파편 이펙트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Effect")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	//몬스터처럼 폰에 맞았을 때 튀는 피 이펙트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Effect")
	TObjectPtr<UNiagaraSystem> BloodEffect;

private:
	//마지막으로 쏜 시각 게임 시작 직후 첫 발이 바로 나가도록 아주 옛날 시각으로 시작
	float LastFireTime = -100.0f;

	//총구 불꽃과 발사음을 재생
	void PlayMuzzleEffects(const FVector& MuzzleLocation);

	//맞은 곳에 이펙트를 재생 폰이면 피 아니면 파편
	void PlayImpactEffect(const FHitResult& Hit);
};
