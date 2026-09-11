// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
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

//계층 밖 클래스
//언리얼 데미지 시스템으로 데미지를 보내는 쪽을 모아둠
//받는 쪽 계산은 PassiveSkillsComponent가 OnTakeAnyDamage에서 처리함
UCLASS()
class DREAMVEIL_API UAugmentDamageLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//한 대상에게 데미지를 보냄 내부에서 UGameplayStatics::ApplyDamage를 부름
	UFUNCTION(BlueprintCallable, Category = "AugmentDamage")
	static float ApplyAugmentDamageToTarget(AActor* DamageCauser, AActor* Target, float Damage);

	//여러 대상에게 같은 데미지를 보냄
	UFUNCTION(BlueprintCallable, Category = "AugmentDamage")
	static void ApplyAugmentDamage(AActor* DamageCauser, const TArray<AActor*>& Targets, float Damage);

	//가시 갑옷 반사 데미지를 보냄 반사 표식이 붙어서 되받아치기가 일어나지 않음
	UFUNCTION(BlueprintCallable, Category = "AugmentDamage")
	static float ApplyThornReflectDamage(AActor* ThornOwner, AActor* Target, float Damage);

	//반사로 들어온 데미지인지 확인
	UFUNCTION(BlueprintCallable, Category = "AugmentDamage")
	static bool IsThornReflectDamage(const UDamageType* DamageType);

	//공격자의 현재 공격력을 가져옴 평타 데미지를 만들 때 씀
	UFUNCTION(BlueprintCallable, Category = "AugmentDamage")
	static float GetOutgoingDamage(AActor* DamageCauser);

	//지정한 범위 안의 대상을 찾음 자기 자신은 제외
	UFUNCTION(BlueprintCallable, Category = "AugmentDamage")
	static void FindTargetsInRadius(AActor* OwnerActor, float Radius, TArray<AActor*>& OutTargets);

private:
	//데미지를 일으킨 컨트롤러를 찾음 킬 판정이나 어그로에 쓰라고 같이 넘김
	static AController* FindEventInstigator(AActor* DamageCauser);
};
