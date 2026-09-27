#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlackHoleZone.generated.h"

class USceneComponent;
class UDecalComponent;
class UMaterialInterface;
class USoundBase;
class UAudioComponent;
class UParticleSystem;

//바닥에 깔리는 블랙홀 장판
//깔린 직후에는 경고만 보이고 ArmDelay 뒤부터 안에 있는 적(플레이어)에게 도트 피해 + 중심으로 약하게 끌어당김
//피해는 화염탄과 같은 UAugmentDamageLibrary::ApplyFireDamage를 씀
//방어력을 무시하고 흡혈 가시 갑옷 반사가 틱마다 터지지 않음
//수치와 머티리얼 사운드는 전부 이 클래스를 부모로 만든 BP에서 설정
//불타는 상태는 화염탄(UContinuousAttackSkill)과 같은 방식으로 대상의 UCombatStatsComponent::SetOnFire를 켜고 끔
//불꽃 에셋도 화염탄처럼 불을 붙인 쪽(시전 몬스터)의 DispatchTableComponent::OnFireEffect를 씀
UCLASS()
class DREAMVEIL_API ABlackHoleZone : public AActor
{
	GENERATED_BODY()

public:
	ABlackHoleZone();

	virtual void Tick(float DeltaSeconds) override;

	//BP에 WarningMaterial을 안 넣었을 때 대신 쓸 경고 머티리얼 스킬이 몬스터의 경고 데칼 머티리얼을 넘겨줌
	void SetFallbackWarningMaterial(UMaterialInterface* Material);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlackHole")
	TObjectPtr<USceneComponent> ZoneRoot;

	//바닥에 투영할 블랙홀 이미지 머티리얼은 아래 ZoneMaterial로 넣음
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BlackHole")
	TObjectPtr<UDecalComponent> ZoneDecal;

	// ---------------- 범위와 시간 ----------------

	//장판 반지름(cm) 데칼 크기와 피해 범위가 같이 바뀜
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole|Zone", meta = (ClampMin = "10"))
	float ZoneRadius = 300.0f;

	//바닥에서 위아래로 이 높이 안에 있어야 장판 안으로 봄 점프로 살짝 떠도 맞게
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole|Zone", meta = (ClampMin = "0"))
	float ZoneHeight = 200.0f;

	//깔린 뒤 실제로 발동하기까지의 경고 시간(초) 이 동안은 피해도 끌어당김도 없음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole|Zone", meta = (ClampMin = "0"))
	float ArmDelay = 1.0f;

	//발동 후 유지 시간(초)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole|Zone", meta = (ClampMin = "0.1"))
	float Duration = 5.0f;

	// ---------------- 피해 ----------------

	//한 번에 주는 화염 피해
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole|Damage", meta = (ClampMin = "0"))
	float DamagePerTick = 5.0f;

	//피해 간격(초) 기본값은 화염탄과 같은 CONTINUOUS_ATTACK_INTERVAL
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole|Damage", meta = (ClampMin = "0.05"))
	float DamageInterval;

	//장판에서 나간 뒤에도 계속 타는 시간(초) 0이면 나가는 즉시 꺼짐
	//화염탄처럼 나가도 잠깐 타게 하려면 CONTINUOUS_ATTACK_DURATION(3초)과 같은 값을 넣으면 됨
	//장판 안에 있는 동안은 매 틱 이 시간이 처음부터 다시 채워짐 화염탄을 다시 맞았을 때와 같은 규칙
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole|Damage", meta = (ClampMin = "0"))
	float BurnLingerSeconds = 0.0f;

	//대상 몸에 붙일 불꽃 비워두면 시전 몬스터의 DispatchTable에 있는 OnFireEffect를 씀 둘 다 비면 불꽃 없이 상태만 켜짐
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole|Damage")
	TObjectPtr<UParticleSystem> OnFireEffectOverride;

	// ---------------- 끌어당김 ----------------

	//중심으로 끌려가는 속도(cm/s) 플레이어 걷기 속도보다 충분히 낮아야 걸어서 빠져나갈 수 있음 0이면 끌어당김 없음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole|Pull", meta = (ClampMin = "0"))
	float PullSpeed = 150.0f;

	//중심에서 이 거리 안이면 더 당기지 않음 한가운데에서 덜덜 떨리는 것 방지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole|Pull", meta = (ClampMin = "0"))
	float PullDeadZone = 30.0f;

	// ---------------- 연출 ----------------

	//발동 중 바닥에 투영할 블랙홀 머티리얼 (Material Domain: Deferred Decal)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole|Visual")
	TObjectPtr<UMaterialInterface> ZoneMaterial;

	//경고 시간 동안 쓸 머티리얼 비워두면 시전한 몬스터의 경고 데칼 머티리얼, 그것도 없으면 ZoneMaterial을 씀
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole|Visual")
	TObjectPtr<UMaterialInterface> WarningMaterial;

	//블랙홀 이미지가 도는 속도(도/초) 0이면 회전 안 함
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole|Visual")
	float SpinSpeed = 45.0f;

	//데칼 투영 깊이(cm) 경사나 계단에서 이미지가 잘리면 키울 것
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole|Visual", meta = (ClampMin = "1"))
	float DecalDepth = 200.0f;

	// ---------------- 사운드 ----------------

	//발동하는 순간 한 번 재생
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole|Sound")
	TObjectPtr<USoundBase> ActivateSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole|Sound", meta = (ClampMin = "0", UIMax = "2"))
	float ActivateSoundVolume = 1.0f;

	//발동 중 반복 재생 사운드 에셋 자체가 루프 설정이어야 계속 남 장판이 사라지면 같이 멈춤
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole|Sound")
	TObjectPtr<USoundBase> LoopSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole|Sound", meta = (ClampMin = "0", UIMax = "2"))
	float LoopSoundVolume = 0.6f;

	//장판 범위를 초록 원통으로 그림 수치 조절할 때만 켤 것
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BlackHole|Debug")
	bool bDrawDebug = false;

private:
	void ActivateZone();
	void ApplyDamageTick();
	void ExpireZone();

	//장판 안에 새로 들어온 대상에게 불을 붙이고 안에 있는 대상은 남은 연소 시간을 다시 채움
	void RefreshBurningTargets(const TArray<AActor*>& InZoneTargets);

	//대상의 불타는 상태를 끄고 목록에서 뺌
	void ExtinguishTarget(AActor* Target);
	void ExtinguishAll();

	//불꽃 에셋 BP 지정 -> 시전 몬스터의 DispatchTable 순서로 찾음
	UParticleSystem* GetOnFireEffect() const;

	//장판이 끝났고 더 탈 대상도 없으면 사라짐
	void DestroyIfDone();

	//장판 안의 살아 있는 적만 모음
	void GatherTargets(TArray<AActor*>& OutTargets) const;
	bool IsHostile(AActor* Target) const;

	void ApplyDecalMaterial(UMaterialInterface* Material);

	FTimerHandle ArmTimer;
	FTimerHandle DamageTimer;
	FTimerHandle DurationTimer;
	bool bZoneActive = false;
	bool bZoneExpired = false;

	//이 장판이 불을 붙인 대상과 장판 밖에서 남은 연소 시간
	//화염탄의 BurningTargets와 같은 역할 대상이 죽거나 사라질 수 있어서 약한 참조
	TMap<TWeakObjectPtr<AActor>, float> BurningTargets;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> FallbackWarningMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> LoopAudio;
};
