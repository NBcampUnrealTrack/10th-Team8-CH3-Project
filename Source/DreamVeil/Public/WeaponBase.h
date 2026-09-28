#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
//EWeaponSlot와 파츠 정의는 WeaponTypes.h로 옮김 이 헤더를 include하면 같이 쓸 수 있음
#include "WeaponTypes.h"
#include "WeaponBase.generated.h"

class UNiagaraSystem;
class UParticleSystem;
class USoundBase;

//총알이 적을 맞혔을 때 알림 이번 발로 대상이 죽었으면 bKilled가 true
//무기가 HUD를 직접 모르게 하려고 알리기만 함 모아서 넘기는 일은 들고 있는 캐릭터가 함
//헤드샷 여부는 사망 레그돌로 머리가 움직이기 전에 판정해서 함께 전달함
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeaponHitConfirmed, bool, bKilled, bool, bHeadshot);

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

	//이 무기가 적을 맞혔을 때 알림 캐릭터가 받아서 HUD의 히트 마커로 넘김
	//벽이나 아군을 맞히면 울리지 않음 실제로 데미지가 들어갔을 때만 울림
	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnWeaponHitConfirmed OnHitConfirmed;

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

	// 파츠

	//이 총에 끼울 수 있는 칸 기본은 모든 총 공용 3칸(총구 탄창 조준기)
	//권총은 이 클래스를 그대로 써서 공용 3칸만 가지고 소총은 override해서 전용 2칸을 더함
	UFUNCTION(BlueprintPure, Category = "Weapon|Part")
	virtual TArray<EWeaponPartSlot> GetPartSlots() const;

	//끼운 파츠를 통째로 바꿈 인벤토리가 장착 강화 파괴 복원 때마다 부름
	void SetEquippedParts(const TArray<FWeaponPart>& NewParts);

	//파츠까지 반영한 실제 발사 간격 CanFire가 이 값으로 연사 간격을 잼
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetFireInterval() const;

	//범위 피해로 맞은 자리에도 같은 피 연출을 냄 폭발탄이 부름
	//총알이 직접 닿은 게 아니라 트레이스 결과가 없으므로 위치만 받음
	//터진 중심에서 바깥으로 튀도록 중심 위치도 같이 받음
	UFUNCTION(BlueprintCallable, Category = "Weapon|Effect")
	void PlaySplashHitEffect(const FVector& Location, const FVector& ExplosionCenter);

	//쏜 사람이 지금 들고 있는 무기 숨겨지지 않은 것이 들고 있는 것
	//증강 스킬이 무기 연출을 빌려 쓸 때 씀 플레이어 클래스를 몰라도 되게 static으로 둠
	//맨손이면 nullptr 로비에서는 쏠 수 없으므로 문제되지 않음
	static UWeaponBase* FindActiveWeapon(const AActor* Shooter);

	//한 발마다 카메라가 위로 튀는 양
	//무기가 직접 카메라를 못 돌리는 이유 무기는 컴포넌트라 컨트롤러를 모름 값만 알려주고 돌리는 건 캐릭터가 함
	UFUNCTION(BlueprintPure, Category = "Weapon|Recoil")
	float GetRecoilPitch() const;

	//한 발마다 카메라가 좌우로 튀는 양 어느 쪽으로 틀지는 쏘는 쪽이 무작위로 정함
	UFUNCTION(BlueprintPure, Category = "Weapon|Recoil")
	float GetRecoilYaw() const;

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

	//한 발마다 카메라가 위로 튀는 양 0이면 반동 없음
	//권총은 한 발이 무거워서 크게 튀고 소총은 연사라 한 발당 작게 둠
	//단위는 각도가 아니라 입력값 컨트롤러가 여기에 감도를 한 번 더 곱해서 실제 각도가 됨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil")
	float RecoilPitch = 1.5f;

	//한 발마다 카메라가 좌우로 튀는 양 매번 좌우 무작위로 들어감
	//위로만 튀면 탄착이 세로 일직선이 되어서 기계가 쏘는 것처럼 보임
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Recoil")
	float RecoilYaw = 0.4f;

	//사격 트레이스 선 맞으면 빨강 빗나가면 초록으로 1초 동안 그림
	//조준 계산을 손볼 때만 켤 것 켜두면 화면이 선으로 덮임
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Debug")
	bool bDrawDebugTrace = false;

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

	//폰이 아닌 곳에 맞았을 때 파편과 같이 겹쳐 재생할 탄착 이펙트
	//파편과 따로 두는 이유 파편은 무엇을 맞혔는지(돌 나무)를 보여주고 이쪽은 어디를 맞혔는지를 보여줘서 역할이 다름
	//같은 에셋을 두 칸에 넣어도 되고 비워두면 기존처럼 하나만 재생됨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Effect")
	TObjectPtr<UNiagaraSystem> ImpactPointEffect;

	//폰에 맞았을 때 피와 같이 겹쳐 재생할 탄착 이펙트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Effect")
	TObjectPtr<UNiagaraSystem> BloodPointEffect;

	//폭발탄 증강을 가졌을 때 위의 추가 효과 두 칸 대신 재생할 폭발
	//겹쳐서 내지 않고 바꿔 치우는 이유 원래 추가 효과(불꽃)와 폭발이 같이 나면 무엇이 터진 건지 안 읽힘
	//비워두면 폭발탄이 있어도 원래 추가 효과가 그대로 나감
	//다른 칸과 달리 Cascade인 이유 쓰려는 폭발 에셋(P_Explosion_Big_A)이 Cascade라 변환 없이 바로 넣으려는 것
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Effect")
	TObjectPtr<UParticleSystem> ExplosionEffect;

	//위 폭발 이펙트가 원래 크기(스케일 1)로 터졌을 때의 반경
	//폭발탄 반경에 맞춰 이펙트를 키우거나 줄이는 기준으로 씀
	//빨간 디버그 구체와 불덩이 크기가 어긋나면 이 값을 실제 크기로 고치면 됨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Effect")
	float ExplosionEffectBaseRadius = 400.0f;

private:
	//마지막으로 쏜 시각 게임 시작 직후 첫 발이 바로 나가도록 아주 옛날 시각으로 시작
	float LastFireTime = -100.0f;

	//지금 끼운 파츠 인벤토리가 넘겨줌 PIE 중에 Details에서 확인할 수 있게 보이게만 함
	UPROPERTY(VisibleInstanceOnly, Category = "Weapon|Part")
	TArray<FWeaponPart> EquippedParts;

	//총구 불꽃과 발사음을 재생
	void PlayMuzzleEffects(const FVector& MuzzleLocation);

	//맞은 곳에 이펙트를 재생 폰이면 피 아니면 파편 거기에 탄착 이펙트를 겹쳐서 둘씩 재생
	void PlayImpactEffect(const FHitResult& Hit);

	//이펙트 하나를 탄착점에 재생 비어 있으면 아무것도 하지 않음
	//네 칸을 같은 규칙으로 재생하려고 뺌 빈 칸 검사를 칸마다 쓰지 않게 됨
	void SpawnImpactEffect(UNiagaraSystem* Effect, const FVector& Location, const FRotator& Rotation);

	//폭발을 탄착점에 재생 피격 반경에 맞춰 크기를 조절함
	//SpawnImpactEffect와 따로 둔 이유 이 칸만 Cascade라 재생 함수가 다름
	void SpawnExplosionEffect(const FVector& Location, const FRotator& Rotation);

	//쏜 사람이 폭발탄 증강을 가졌는지 무기는 증강을 모르므로 증강 컴포넌트에 물어봄
	bool HasAreaAttackAugment() const;
};
