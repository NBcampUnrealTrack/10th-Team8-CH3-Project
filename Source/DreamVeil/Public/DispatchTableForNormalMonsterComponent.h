// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AugmentTypes.h"
#include "DispatchTableForNormalMonsterComponent.generated.h"

class UPassiveSkillsComponent;

//3계층
//일반 몬스터는 패시브만 쓰기 때문에 패시브 전용 디스패치 테이블
//일반 몬스터는 뽑기를 하지 않으므로 4계층 없이 5계층이 이걸 바로 들고 있음
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DREAMVEIL_API UDispatchTableForNormalMonsterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	//생성자
	UDispatchTableForNormalMonsterComponent();

	//번호로 패시브 증강 효과를 실행
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void ExecuteAugment(EAugmentID AugmentID);

	//정해진 증강 묶음을 한 번에 적용 타입별 고정 패시브용
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void ExecuteAugments(const TArray<EAugmentID>& AugmentIDs);

	//같은 증강을 여러 번 적용 엔드리스 웨이브 진행도에 따른 체공방 증가용
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void ExecuteAugmentRepeat(EAugmentID AugmentID, int32 RepeatCount);

	//테이블에 등록된 증강인지 확인
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool HasAugment(EAugmentID AugmentID);

	//패시브 스킬 Getter
	UFUNCTION(BlueprintCallable, Category = "Augment")
	UPassiveSkillsComponent* GetPassiveSkillsComponent();

private:
	//2계층 패시브 스킬
	UPROPERTY(VisibleAnywhere, Category = "Augment")
	TObjectPtr<UPassiveSkillsComponent> PassiveSkillsComponent;

	//증강 번호와 패시브 효과 함수를 짝지은 테이블
	TMap<EAugmentID, TFunction<void()>> PassiveAugmentMap;

	//증강 효과 함수들을 테이블에 등록
	void RegisterPassiveAugmentFunctions();
};
