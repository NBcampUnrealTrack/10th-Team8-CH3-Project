#pragma once

#include "CoreMinimal.h"
#include "MonsterProjectile.h"
#include "ExplosiveMonsterProjectile.generated.h"

class UParticleSystem;
class USoundBase;
class USoundAttenuation;

//맞은 자리에서 터지는 몬스터 투사체
//플레이어 폭발탄(UAreaAttackSkill)과 같은 규칙을 씀
//직격 대상은 온전한 피해, 주변 대상은 피해 비율 x 거리 감쇠만큼 받음
//대상 탐색 아군 판정 피해 전송은 전부 AugmentDamageLibrary를 그대로 재사용함
UCLASS()
class DREAMVEIL_API AExplosiveMonsterProjectile : public AMonsterProjectile
{
	GENERATED_BODY()

public:
	AExplosiveMonsterProjectile();

protected:
	virtual void ProcessHit(AActor* OtherActor, const FHitResult& Hit) override;

	//폭발 반경 기본값은 플레이어 폭발탄과 같은 AREA_ATTACK_RADIUS
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Explosion", meta = (ClampMin = "0"))
	float ExplosionRadius;

	//폭발 한가운데에서 받는 피해 비율 기본값은 AREA_ATTACK_DAMAGE_RATIO
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Explosion", meta = (ClampMin = "0"))
	float SplashDamageRatio;

	//폭발 가장자리에서도 남는 최소 비율 기본값은 AREA_ATTACK_MIN_FALLOFF
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Explosion", meta = (ClampMin = "0", ClampMax = "1"))
	float SplashMinFalloff;

	//터질 때 재생할 폭발 이펙트 플레이어 무기와 같은 Cascade 에셋(P_Explosion_Big_A 등)을 넣으면 됨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Explosion")
	TObjectPtr<UParticleSystem> ExplosionEffect;

	//위 이펙트가 스케일 1일 때의 반경 폭발 반경에 맞춰 이펙트 크기를 조절하는 기준 무기의 같은 이름 값과 동일
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Explosion", meta = (ClampMin = "1"))
	float ExplosionEffectBaseRadius = 400.0f;

	//터질 때 재생할 소리 비워두면 소리 없이 터짐
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Explosion|Sound")
	TObjectPtr<USoundBase> ExplosionSound;

	//폭발 소리 볼륨 배율 1이 원본 크기
	//부채꼴처럼 여러 발이 거의 동시에 터지면 소리가 겹쳐 커지므로 그때는 낮춰서 쓸 것
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Explosion|Sound", meta = (ClampMin = "0", UIMin = "0", UIMax = "2"))
	float ExplosionSoundVolume = 1.0f;

	//폭발 소리 피치 배율 1이 원본 약간씩 바꾸면 같은 소리가 반복돼도 덜 단조로움
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Explosion|Sound", meta = (ClampMin = "0.1", UIMin = "0.5", UIMax = "2"))
	float ExplosionSoundPitch = 1.0f;

	//거리에 따른 감쇠 설정 비워두면 사운드 에셋에 지정된 감쇠를 그대로 씀
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Explosion|Sound")
	TObjectPtr<USoundAttenuation> ExplosionSoundAttenuation;

	//터진 자리와 반경을 빨간 구체로 잠깐 그림 수치 조절할 때만 켤 것
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Explosion")
	bool bDrawExplosionDebug;

private:
	//쏜 몬스터 기준으로 적인지 판정 쏜 사람을 못 찾으면 몬스터가 아닌 대상만 적으로 봄
	bool IsHostileTarget(AActor* Shooter, AActor* Target) const;

	//중심 주변 대상에게 거리 감쇠된 범위 피해 직격 대상은 제외
	void ApplySplashDamage(AActor* Shooter, const FVector& Center, AActor* DirectHitActor);

	//폭발 이펙트를 반경에 맞춘 크기로 재생
	void SpawnExplosionEffect(const FVector& Location, const FRotator& Rotation) const;

	//폭발 소리를 터진 자리에서 재생
	void PlayExplosionSound(const FVector& Location) const;
};
