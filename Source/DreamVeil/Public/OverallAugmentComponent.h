// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AugmentTypes.h"
#include "OverallAugmentComponent.generated.h"

class UDispatchTableForPlayerAndBossComponent;

//증강이 적용됐을 때 알리는 이벤트 보상 UI가 씀
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnAugmentApplied,
	EAugmentID, AugmentID
);

//4계층 공통 부분
//플레이어와 보스가 똑같이 쓰는 풀 관리와 뽑기를 여기에 모아둠
//이 클래스를 직접 붙이지 말고 플레이어용 보스용 자식 클래스를 붙일 것
UCLASS(Abstract, ClassGroup = (Custom))
class DREAMVEIL_API UOverallAugmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	//생성자
	UOverallAugmentComponent();

	//증강이 적용됐을 때 알리는 이벤트
	UPROPERTY(BlueprintAssignable, Category = "AugmentEvent")
	FOnAugmentApplied OnAugmentApplied;

	//뽑을 수 있는 증강 목록
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
	TArray<FAugmentData> AugmentPool;

	//가중치 누적합으로 증강 하나를 뽑음 성공하면 true
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool DrawWeightedAugmentID(EAugmentID& OutAugmentID);

	//증강을 적용하고 반복 획득 불가면 풀에서 제거
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void ApplyAugment(EAugmentID AugmentID);

	//3계층 디스패치 테이블로 실행을 위임
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void ExecuteAugment(EAugmentID AugmentID);

	//정해진 증강 묶음을 한 번에 적용 고정 세트용
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void ApplyFixedAugments(const TArray<EAugmentID>& AugmentIDs);

	//풀에 남은 증강 개수
	UFUNCTION(BlueprintCallable, Category = "Augment")
	int32 GetRemainingAugmentCount();

	//3계층 디스패치 테이블 Getter
	UFUNCTION(BlueprintCallable, Category = "Augment")
	UDispatchTableForPlayerAndBossComponent* GetDispatchTable();

protected:
	//3계층 디스패치 테이블
	UPROPERTY(VisibleAnywhere, Category = "Augment")
	TObjectPtr<UDispatchTableForPlayerAndBossComponent> DispatchTableForPlayerAndBoss;

	//기본 증강 풀을 채움 에디터에서 따로 채워두면 건너뜀
	//생성자에서 부르므로 virtual이 아님 자식이 다른 풀을 쓰려면 자기 생성자에서 AugmentPool을 고칠 것
	void BuildDefaultAugmentPool();

	//뽑기에 쓸 실제 가중치 보스는 웨이브 레벨을 곱하려고 재정의함
	virtual float GetDrawWeight(const FAugmentData& AugmentData);

	//번호로 증강 정보를 찾음 없으면 nullptr
	FAugmentData* FindAugmentData(EAugmentID AugmentID);
};
