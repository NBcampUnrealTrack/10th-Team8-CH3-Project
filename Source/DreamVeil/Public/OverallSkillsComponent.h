// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AugmentTypes.h"
#include "OverallSkillsComponent.generated.h"

class AActor;
class UPassiveSkillsComponent;
class UActiveSkillsComponent;

//증강이 적용됐을 때 알리는 이벤트 보상 UI가 씀
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnAugmentApplied,
	EAugmentID, AugmentID
);

//누구에게 얼마의 데미지를 줄지 알리는 이벤트 플레이어 및 데미지 쪽에서 받아서 실제로 적용
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnAugmentDamageRequested,
	const TArray<AActor*>&, Targets,
	float, Damage
);

//무기 증강이 실행됐을 때 알리는 이벤트 5계층에서 무기 쪽이 받아감
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnWeaponAugmentExecuted,
	EAugmentID, AugmentID
);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DREAMVEIL_API UOverallSkillsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	//생성자
	UOverallSkillsComponent();

	//증강이 적용됐을 때 알리는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "AugmentEvent")
	FOnAugmentApplied OnAugmentApplied;

	//데미지 적용 요청 이벤트
	UPROPERTY(BlueprintAssignable, Category = "AugmentEvent")
	FOnAugmentDamageRequested OnAugmentDamageRequested;

	//무기 증강 실행 이벤트 무기 컴포넌트가 생기면 여기에 붙임
	UPROPERTY(BlueprintAssignable, Category = "AugmentEvent")
	FOnWeaponAugmentExecuted OnWeaponAugmentExecuted;

	//뽑을 수 있는 증강 목록
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
	TArray<FAugmentData> AugmentPool;

	//가중치 누적합으로 증강 하나를 뽑음 성공하면 true
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool DrawWeightedAugmentID(EAugmentID& OutAugmentID);

	//증강을 적용하고 비중첩이면 풀에서 제거
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void ApplyAugment(EAugmentID AugmentID);

	//분류를 보고 패시브 액티브 무기 중 맞는 쪽으로 위임
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void ExecuteAugment(EAugmentID AugmentID);

	//뽑아서 바로 적용 보상 단계에서 부르는 외부 진입점
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool DrawAndApplyAugment();

	//정해진 증강 묶음을 한 번에 적용 일반 정예 몬스터 고정 세트용
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void ApplyFixedAugments(const TArray<EAugmentID>& AugmentIDs);

	//풀에 남은 증강 개수
	UFUNCTION(BlueprintCallable, Category = "Augment")
	int32 GetRemainingAugmentCount();

private:
	//패시브 담당 2계층
	UPROPERTY(VisibleAnywhere, Category = "Augment")
	TObjectPtr<UPassiveSkillsComponent> PassiveSkillsComponent;

	//액티브 담당 2계층
	UPROPERTY(VisibleAnywhere, Category = "Augment")
	TObjectPtr<UActiveSkillsComponent> ActiveSkillsComponent;

	//기본 증강 풀을 채움 에디터에서 따로 채워두면 건너뜀
	void BuildDefaultAugmentPool();

	//소유 액터에 붙어 있는 패시브 액티브 컴포넌트를 찾음
	void FindSkillComponents();

	//번호로 증강 정보를 찾음 없으면 nullptr
	FAugmentData* FindAugmentData(EAugmentID AugmentID);

	//액티브가 올린 데미지 요청을 그대로 위로 넘김
	UFUNCTION()
	void HandleActiveDamageRequested(const TArray<AActor*>& Targets, float Damage);

	//생명주기 함수
	virtual void BeginPlay() override;
};
