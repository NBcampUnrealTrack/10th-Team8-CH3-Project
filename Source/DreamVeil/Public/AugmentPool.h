#pragma once

#include "CoreMinimal.h"
#include "AugmentTypes.h"
#include "AugmentPool.generated.h"

//증강 하나의 정보 풀에 담기는 단위
USTRUCT(BlueprintType)
struct FAugmentData
{
	GENERATED_BODY()

	//증강 번호
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
	EAugmentID AugmentID = EAugmentID::AttackUp;

	//증강 분류
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
	EAugmentCategory Category = EAugmentCategory::Passive;

	//뽑기 가중치 클수록 잘 나옴
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
	float Weight = 1.0f;

	//반복 획득 가능 여부 false면 한 번 뽑힌 뒤 풀에서 제거
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
	bool bRepeatable = false;

	//보상 UI에 띄울 이름
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
	FText DisplayName;

	//보상 UI에 띄울 효과 설명 BuildDefault에서 채움
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
	FText Description;

	//기본 생성자
	FAugmentData() {}

	//값을 채워 만드는 생성자
	FAugmentData(EAugmentID InAugmentID, EAugmentCategory InCategory, float InWeight, bool bInRepeatable)
		: AugmentID(InAugmentID)
		, Category(InCategory)
		, Weight(InWeight)
		, bRepeatable(bInRepeatable)
	{
	}
};

//증강 풀과 가중치 뽑기
//DispatchTableComponent가 들고 있어서 에디터 디테일 패널에서 목록을 바로 고칠 수 있음
//플레이어와 보스가 같이 쓰고 일반 몬스터는 뽑기를 부르지 않음
USTRUCT(BlueprintType)
struct FAugmentPool
{
	GENERATED_BODY()

	//뽑을 수 있는 증강 목록
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
	TArray<FAugmentData> Augments;

	//기본 증강 목록을 채움 이미 채워져 있으면 건너뜀 플레이어용
	void BuildDefault();

	//보스가 뽑을 증강만 채움 이미 들어 있던 목록은 비우고 다시 채움
	//BuildDefault처럼 "비어 있을 때만"으로 두지 않는 이유
	//컴포넌트 생성자가 이미 플레이어용 목록을 채워둬서 그냥 두면 그게 그대로 남음
	//스태미나 넉백 범위공격 지속공격을 빼는 이유 총을 쏘고 달리는 플레이어에게만 뜻이 있는 것들이라
	//보스가 뽑으면 아무 일도 안 일어나는 빈 칸이 됨
	void BuildBossDefault();

	//번호로 증강 정보를 찾음 없으면 nullptr
	const FAugmentData* Find(EAugmentID AugmentID) const;

	//가중치 누적합으로 겹치지 않게 Count개를 뽑음 풀은 건드리지 않음 하나라도 뽑으면 true
	bool DrawChoices(int32 Count, TArray<EAugmentID>& OutAugmentIDs) const;

	//반복 획득이 안 되는 증강이면 풀에서 제거
	void RemoveIfNotRepeatable(EAugmentID AugmentID);

	//목록의 이름과 설명을 채움 목록을 만드는 곳마다 같은 반복문을 복사하지 않으려고 뺌
	void FillDisplayTexts();
};
