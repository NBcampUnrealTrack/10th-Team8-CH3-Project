// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterSpawnVolume.h"
#include "Kismet/GameplayStatics.h"

#include "MainPlayerCharacter.h"
#include "MonsterBase.h"
#include "Components/CapsuleComponent.h"
#include "DreamVeilGameInstance.h"

AMonsterSpawnVolume::AMonsterSpawnVolume()
{
	MaxDifficultyLevel = 30;
	EliteMonsterMinRate = 5.0f;
	EliteMonsterMaxRate = 50.0f;
	DifficultyCurve = 1.0f;
}

void AMonsterSpawnVolume::ExecuteSpawnActor()
{
	//기존 호출 진입점은 유지하고 생성 성공 판정은 한 함수로 모음
	TrySpawnMonster();
}

bool AMonsterSpawnVolume::TrySpawnMonster()
{
	FVector SpawnLocation;
	if (!TryGetRandomNavLocation(SpawnLocation)) return false;
	//배치된 몬스터 인스턴스가 아니라 BP 클래스를 선택해야 새 액터를 생성할 수 있음
	const TArray<TSubclassOf<AMonsterBase>>& Classes = !EliteMonsters.IsEmpty()
		&& FMath::FRandRange(0.0f, 100.0f) < GetEliteRate() ? EliteMonsters : BaseMonsters;
	if (Classes.IsEmpty()) return false;
	const TSubclassOf<AMonsterBase> MonsterClass = Classes[FMath::RandRange(0, Classes.Num() - 1)];
	if (!MonsterClass) return false;
	//NavMesh 좌표는 바닥이므로 캡슐 반높이만큼 올려 바닥과 몸통이 겹치지 않게 함
	SpawnLocation.Z += MonsterClass->GetDefaultObject<AMonsterBase>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FActorSpawnParameters SpawnParameters;
	//시간 간격만으로 겹침이 보장되지는 않음. 공간이 없으면 생성을 취소하고 다음 간격에 재시도함
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	AMonsterBase* Monster = GetWorld()->SpawnActor<AMonsterBase>(MonsterClass, SpawnLocation, FRotator::ZeroRotator, SpawnParameters);
	if (!IsValid(Monster)) return false;
	if (!Monster->GetController()) Monster->SpawnDefaultController();
	return true;
}
void AMonsterSpawnVolume::BeginPlay()
{
	Super::BeginPlay();

	PlayerPawn = Cast<AMainPlayerCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));

	MaxDifficultyLevel = FMath::Max(MaxDifficultyLevel, 2);

	EliteMonsterMinRate = FMath::Clamp(
		EliteMonsterMinRate,
		0.0f,
		99.0f
	);

	EliteMonsterMaxRate = FMath::Clamp(
		EliteMonsterMaxRate,
		EliteMonsterMinRate + 1.0f,
		100.0f
	);

	DifficultyCurve = FMath::Clamp(
		DifficultyCurve,
		0.1f,
		5.0f
	);
}

float AMonsterSpawnVolume::GetEliteRate()
{
	if (!PlayerPawn.IsValid()) PlayerPawn = Cast<AMainPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!PlayerPawn.IsValid()) return 0;
	int32 PlayerLevel = PlayerPawn->GetPlayerLevel();

	float LevelAlpha = FMath::Clamp(
		static_cast<float>(PlayerLevel - 1) / (MaxDifficultyLevel - 1),
		0.0f,
		1.0f
	);
	
	// 게임 난이도가 이 경사를 눕히거나 세움. 쉬움이면 엘리트가 늦게 나오고 어려움이면 빨리 나옴
	// 최소 최대 스폰율은 그대로 둬서 위에 잡아둔 값의 뜻이 바뀌지 않음
	float CurveExponent = DifficultyCurve;

	if (const UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>())
	{
		CurveExponent *= DreamVeilGameInstance->GetSpawnCurveScale();
	}

	LevelAlpha = FMath::Pow(LevelAlpha, CurveExponent);

	return FMath::Lerp(EliteMonsterMinRate, EliteMonsterMaxRate, LevelAlpha);
}

//이전 구조 학습 메모: 아래 주석은 원문 보존용이며 현재 구현 설명이 아님
//엘리트 스폰
//일반 몬스터 스폰
// 웨이브 스폰
// 게임모드가 "이번 웨이브에 몇 마리 내라"고 시키면 여기서 간격을 두고 내보냄
// 실제로 한 마리를 만드는 일은 ExecuteSpawnActor가 하므로 그쪽만 고치면 스폰 방식이 바뀜
//웨이브 시작 남은 수를 더하고 타이머를 깨움
//앞 웨이브가 아직 다 안 나왔으면 남은 수에 더함 타이머가 이어서 마저 내보냄
//이미 돌고 있으면 그대로 두고 남은 수만 늘어남 타이머를 다시 걸면 간격이 흐트러짐
//0 이하가 들어오면 타이머가 매 프레임 돌아서 최소값으로 막음
//첫 마리도 간격만큼 기다렸다 나옴 웨이브 시작과 동시에 눈앞에 튀어나오지 않게
//타이머가 돌 때마다 한 마리 내보냄
//마지막 한 마리를 냈으면 더 돌 이유가 없음
