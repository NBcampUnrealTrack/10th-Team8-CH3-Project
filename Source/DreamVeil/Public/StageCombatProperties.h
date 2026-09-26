// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StageCombatProperties.generated.h"

class AMonsterBase;

// 스테이지 한 판에 쓸 설정 묶음. 웨이브마다 스탯 바꾸지 말고 같은 값 전달할 용도
USTRUCT(BlueprintType)
struct FStageCombatProperties
{
	GENERATED_BODY()

	//레벨 시작 때 예약할 전체 마릿수. 볼륨 개수만큼 곱하지 않음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0"))
	int32 InitialMonsterCount = 4;
	//이 시간마다 기본 수량에 증가량을 누적하여 새로운 소환 묶음을 예약함
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0.1", Units = "s"))
	float SpawnIncreaseInterval = 30.0f;
	//기본 4, 증가 4라면 0초 4마리 / 30초 8마리 / 60초 12마리를 각각 추가 예약함
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0"))
	int32 MonsterCountIncrease = 4;
	//여러 볼륨을 통틀어 한 마리씩 생성할 간격. 충돌 검사도 별도로 수행함
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0.05", Units = "s"))
	float MonsterSpawnInterval = 0.5f;
	//경과 시간이 여기에 도달하면 일반 소환 예약을 취소하고 보스를 한 번 생성함
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss", meta = (ClampMin = "0.1", Units = "s"))
	float BossSpawnTime = 150.0f;


	// 마지막 웨이브에 낼 보스 BP 넣는 곳. 안 넣으면 게임모드가 진행 안 함
	// 패턴 누적 같은 건 이 변수만 넣는다고 되는 거 아님. 보스 쪽 구현은 별도로 필요함
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AMonsterBase> BossClass;
};

//이전 구조 학습 메모: 아래 주석은 원문 보존용이며 현재 구현 설명이 아님
// 마지막 보스까지 포함한 횟수. 일반은 6, 엔드리스는 3으로 설정하면 됨
// 일반 전투 한 번에 줄 시간. 이 안에 다 잡으면 남은 시간은 스킵함
// 볼륨 하나당 말고 웨이브 전체 마릿수임. 숫자는 임시로 둔 거라 맵별로 조절 ㄱㄱ
// 0번이 첫 웨이브고 보스 수량은 안 넣음. 총 웨이브보다 한 칸 적어야 됨
// 스테이지 공통 체력 최솟값. 지금은 전달만 하고 몬스터에 적용하는 건 아직 안 붙임
// 0은 기존 클래스 범위 쓰는 용도로 남겨둔 값. 실제 적용할 때 그 규칙도 연결해야 됨
// 위 최솟값이랑 짝인 최댓값. 난이도별 증가량 계산도 나중에 연결할 자리
//TSubclassOf로 클래스만 저장해둠. 엘리트랑 보스 클래스 블프 지정 저장용
