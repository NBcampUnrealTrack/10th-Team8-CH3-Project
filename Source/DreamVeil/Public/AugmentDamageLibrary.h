// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "Engine/HitResult.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AugmentDamageLibrary.generated.h"

class AActor;
class AController;

//가시 갑옷 반사 데미지에 붙이는 표식
//이 타입으로 들어온 데미지는 다시 반사하지 않고 흡혈도 시키지 않음
//두 캐릭터가 서로 가시 갑옷을 들고 있을 때 무한 반사되는 것을 막기 위함
UCLASS()
class DREAMVEIL_API UThornReflectDamageType : public UDamageType
{
	GENERATED_BODY()
};

//지속 공격 화염 데미지에 붙이는 표식
//이 타입으로 들어온 데미지는 대상 방어력을 무시하고 흡혈과 가시 갑옷 반사를 일으키지 않음
//틱마다 가시 갑옷 반사가 들어와 쏜 사람이 계속 깎이는 것을 막기 위함
UCLASS()
class DREAMVEIL_API UFireDamageType : public UDamageType
{
	GENERATED_BODY()
};

//헤드샷 데미지 배율 머리를 맞히면 데미지가 이만큼 곱해짐
//총 적중(ApplyWeaponHit)만 이 배율을 받음 범위 공격 화염 가시 갑옷 반사는 머리를 겨눌 수 있는 공격이 아니라서 그냥 평타로 들어감
const float HEADSHOT_DAMAGE_MULTIPLIER = 2.0f;

//언리얼 데미지 시스템으로 데미지를 주고받는 처리를 모아둠
//보내는 쪽은 ApplyAugmentDamageToTarget 총 적중은 ApplyWeaponHit
//받는 쪽은 캐릭터 TakeDamage에서 Super::TakeDamage 뒤에 ProcessIncomingDamage를 부름
UCLASS()
class DREAMVEIL_API UAugmentDamageLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//받은 데미지를 순서대로 처리 방어력 차감 -> 체력 적용 -> 흡혈 -> 가시 갑옷 반사
	//캐릭터 TakeDamage가 부르고 반환값을 그대로 TakeDamage의 반환값으로 쓸 것
	UFUNCTION(BlueprintCallable, Category = "AugmentDamage")
	static float ProcessIncomingDamage(AActor* DamagedActor, float Damage, TSubclassOf<UDamageType> DamageTypeClass, AController* EventInstigator, AActor* DamageCauser);

	//한 대상에게 데미지를 보냄 내부에서 UGameplayStatics::ApplyDamage를 부름
	UFUNCTION(BlueprintCallable, Category = "AugmentDamage")
	static float ApplyAugmentDamageToTarget(AActor* DamageCauser, AActor* Target, float Damage);

	//여러 대상에게 같은 데미지를 보냄
	UFUNCTION(BlueprintCallable, Category = "AugmentDamage")
	static void ApplyAugmentDamage(AActor* DamageCauser, const TArray<AActor*>& Targets, float Damage);

	//총이 무언가를 맞혔을 때 부름 맞은 대상에게 데미지를 보내고 쏜 사람의 적중 증강(범위 공격 감속 지속 공격)을 발동
	//히트스캔은 DamageCauser에 쏜 캐릭터 투사체는 투사체 자신을 넘김 투사체는 스폰할 때 Instigator 필수
	//빗나간 결과나 아군 자기 자신을 맞힌 결과는 아무것도 하지 않고 0을 돌려줌
	UFUNCTION(BlueprintCallable, Category = "AugmentDamage")
	static float ApplyWeaponHit(AActor* DamageCauser, const FHitResult& HitResult, float Damage);

	//가시 갑옷 반사 데미지를 보냄 반사 표식이 붙어서 되받아치기가 일어나지 않음
	UFUNCTION(BlueprintCallable, Category = "AugmentDamage")
	static float ApplyThornReflectDamage(AActor* ThornOwner, AActor* Target, float Damage);

	//반사로 들어온 데미지인지 확인
	UFUNCTION(BlueprintCallable, Category = "AugmentDamage")
	static bool IsThornReflectDamage(TSubclassOf<UDamageType> DamageTypeClass);

	//지속 공격 화염 데미지를 보냄 화염 표식이 붙어서 흡혈과 가시 갑옷 반사가 일어나지 않음
	UFUNCTION(BlueprintCallable, Category = "AugmentDamage")
	static float ApplyFireDamage(AActor* DamageCauser, AActor* Target, float Damage);

	//화염으로 들어온 데미지인지 확인
	UFUNCTION(BlueprintCallable, Category = "AugmentDamage")
	static bool IsFireDamage(TSubclassOf<UDamageType> DamageTypeClass);

	//공격자의 현재 공격력을 가져옴 평타 데미지를 만들 때 씀
	UFUNCTION(BlueprintCallable, Category = "AugmentDamage")
	static float GetOutgoingDamage(AActor* DamageCauser);

	//지정한 범위 안의 대상을 찾음 자기 자신은 제외
	UFUNCTION(BlueprintCallable, Category = "AugmentDamage")
	static void FindTargetsInRadius(AActor* OwnerActor, float Radius, TArray<AActor*>& OutTargets);

	//원하는 위치 기준 범위 안의 대상을 찾음 ActorsToIgnore에 넣은 대상은 제외 아군 적군은 가리지 않음
	UFUNCTION(BlueprintCallable, Category = "AugmentDamage", meta = (WorldContext = "WorldContextObject"))
	static void FindTargetsAtLocation(UObject* WorldContextObject, FVector Location, float Radius, const TArray<AActor*>& ActorsToIgnore, TArray<AActor*>& OutTargets);

	//두 액터가 서로 적인지 확인 몬스터끼리는 아군 몬스터가 아닌 쪽(플레이어)과 몬스터는 적
	UFUNCTION(BlueprintPure, Category = "AugmentDamage")
	static bool IsEnemy(AActor* ActorA, AActor* ActorB);

private:
	//데미지를 일으킨 컨트롤러를 찾음 킬 판정이나 어그로에 쓰라고 같이 넘김
	static AController* FindEventInstigator(AActor* DamageCauser);

	//흡혈과 반사를 받을 실제 공격자를 찾음 투사체가 때렸으면 쏜 폰을 돌려줌
	static AActor* FindAttacker(AController* EventInstigator, AActor* DamageCauser);
};
