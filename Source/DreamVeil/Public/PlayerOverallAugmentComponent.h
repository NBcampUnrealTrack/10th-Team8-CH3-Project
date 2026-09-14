// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AugmentTypes.h"
#include "PlayerOverallAugmentComponent.generated.h"

class UDispatchTableForPlayerAndBossComponent;

//4계층 플레이어용
//증강 풀을 들고 가중치로 뽑아서 3계층 디스패치 테이블로 넘김
//보상 UI 선택지는 PlayerDrawAugmentChoices로 뽑고 고른 것만 ApplyAugment
//선택 없이 바로 적용할 때는 DrawAndApplyAugment
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DREAMVEIL_API UPlayerOverallAugmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	//생성자
	UPlayerOverallAugmentComponent();

	//뽑을 수 있는 증강 목록
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
	TArray<FAugmentData> AugmentPool;

	//가중치 누적합으로 증강 하나를 뽑음 성공하면 true
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool PlayerDrawWeightedAugmentID(EAugmentID& OutAugmentID);

	//보상 UI 선택지용으로 겹치지 않게 AUGMENT_CHOICE_COUNT개를 뽑음 풀은 건드리지 않음
	//풀에 남은 증강이 모자라면 있는 만큼만 담김 하나라도 뽑으면 true
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool PlayerDrawAugmentChoices(TArray<EAugmentID>& OutAugmentIDs);

	//증강을 적용하고 반복 획득 가능 여부가 false면 한 번 뽑힌 뒤 풀에서 제거
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void ApplyAugment(EAugmentID AugmentID);

	//Category를 보고 디스패치 테이블로 실행을 위임
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void ExecuteAugment(EAugmentID AugmentID);

	//뽑아서 바로 적용 외부 진입점
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool DrawAndApplyAugment();

private:
	//Has-A 3계층 디스패치 테이블
	UPROPERTY(VisibleAnywhere, Category = "Augment")
	TObjectPtr<UDispatchTableForPlayerAndBossComponent> DispatchTableForPlayerAndBoss;

	//기본 증강 풀을 채움 에디터에서 따로 채워두면 건너뜀
	void BuildDefaultAugmentPool();

	//번호로 증강 정보를 찾음 없으면 nullptr
	FAugmentData* FindAugmentData(EAugmentID AugmentID);
};
