// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OverallAugmentComponent.h"
#include "PlayerOverallAugmentComponent.generated.h"

//4계층 플레이어용
//보상 UI에서 DrawAndApplyAugment 하나만 부르면 뽑기부터 적용까지 끝남
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DREAMVEIL_API UPlayerOverallAugmentComponent : public UOverallAugmentComponent
{
	GENERATED_BODY()

public:
	//생성자
	UPlayerOverallAugmentComponent();

	//플레이어 가중치로 증강 하나를 뽑음 보상 UI에 후보를 띄울 때 씀
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool PlayerDrawWeightedAugmentID(EAugmentID& OutAugmentID);

	//겹치지 않는 후보 여러 개를 뽑음 보상 UI에 선택지 세 개를 띄울 때 씀
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void PlayerDrawAugmentChoices(int32 ChoiceCount, TArray<EAugmentID>& OutAugmentIDs);

	//뽑아서 바로 적용 외부 진입점
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool DrawAndApplyAugment();
};
