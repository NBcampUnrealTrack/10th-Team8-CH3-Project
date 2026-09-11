// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OverallAugmentComponent.h"
#include "BossMonsterOverallAugmentComponent.generated.h"

//4계층 보스용
//엔드리스 단계에 따라 강화되는 것은 가중치와 레벨로 처리
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DREAMVEIL_API UBossMonsterOverallAugmentComponent : public UOverallAugmentComponent
{
	GENERATED_BODY()

public:
	//생성자
	UBossMonsterOverallAugmentComponent();

	//엔드리스 웨이브 레벨 높을수록 공방체 증가 쪽이 잘 뽑힘
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
	int32 WaveLevel;

	//웨이브 레벨당 공방체 증가 가중치에 더해지는 값
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
	float StatWeightPerWaveLevel;

	//웨이브 레벨을 설정
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void SetWaveLevel(int32 NewWaveLevel);

	//보스 가중치로 증강 하나를 뽑음
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool BossWeightedAugmentID(EAugmentID& OutAugmentID);

	//뽑아서 바로 적용 외부 진입점
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool RandomApplyAugment();

	//웨이브가 넘어갈 때 레벨을 올리고 그만큼 증강을 뽑아 적용
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void AdvanceWave(int32 NewWaveLevel, int32 AugmentCount);

protected:
	//뽑기에 쓸 실제 가중치 웨이브 레벨이 오를수록 공방체 증가 쪽을 밀어줌
	virtual float GetDrawWeight(const FAugmentData& AugmentData) override;
};
