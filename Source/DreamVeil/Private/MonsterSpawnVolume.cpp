// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterSpawnVolume.h"
#include "Kismet/GameplayStatics.h"

#include "MainPlayerCharacter.h"
#include "MonsterBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "TimerManager.h"
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
	FVector SpawnLocation;

	//땅 위 설 수 있는 자리를 못 찾으면 이번 한 마리는 건너뜀
	//공중이나 벽 속에 내면 몬스터가 끼거나 떨어져서 플레이어에게 오지 못함
	if (!TryGetRandomNavLocation(SpawnLocation))
	{
		return;
	}

	const float EliteSpawnRate = GetEliteRate();

	//엘리트에 당첨됐는데 엘리트 목록이 비어 있으면 잡몹으로 내려감
	//여기서 그냥 돌아가면 엘리트를 안 채워둔 볼륨이 아무것도 안 내서 웨이브가 통째로 비어버림
	if (FMath::FRandRange(0.0f, 100.0f) <= EliteSpawnRate && EliteMonsters.Num() > 0)
	{
		SpawnOneMonster(EliteMonsters, SpawnLocation);
		return;
	}

	SpawnOneMonster(BaseMonsters, SpawnLocation);
}

AMonsterBase* AMonsterSpawnVolume::SpawnOneMonster(const TArray<TSubclassOf<AMonsterBase>>& MonsterClasses, const FVector& SpawnLocation)
{
	//낼 종류를 안 채워뒀으면 낼 것이 없음
	if (MonsterClasses.Num() == 0)
	{
		return nullptr;
	}

	//비어 있는 칸을 뽑을 수 있으므로 채워진 것만 모아서 고름
	//블루프린트에서 배열 칸만 늘리고 클래스를 안 넣어두는 일이 흔함
	TArray<TSubclassOf<AMonsterBase>> ValidClasses;

	for (const TSubclassOf<AMonsterBase>& MonsterClass : MonsterClasses)
	{
		if (MonsterClass)
		{
			ValidClasses.Add(MonsterClass);
		}
	}

	if (ValidClasses.Num() == 0)
	{
		return nullptr;
	}

	const TSubclassOf<AMonsterBase> ChosenClass = ValidClasses[FMath::RandRange(0, ValidClasses.Num() - 1)];

	//캡슐 절반 높이만큼 띄워서 냄 바닥에 파묻힌 채로 나오면 이동 컴포넌트가 밀어내느라 튕김
	FVector AdjustedLocation = SpawnLocation;

	if (const ACharacter* MonsterDefault = ChosenClass->GetDefaultObject<ACharacter>())
	{
		if (const UCapsuleComponent* DefaultCapsule = MonsterDefault->GetCapsuleComponent())
		{
			AdjustedLocation.Z += DefaultCapsule->GetScaledCapsuleHalfHeight();
		}
	}

	FActorSpawnParameters SpawnParameters;

	//좁은 곳이라 겹쳐도 일단 냄 안 그러면 몬스터가 몰린 웨이브에서 스폰이 통째로 실패함
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	//플레이어 쪽을 보고 나오게 함 등을 돌린 채 나와서 한 바퀴 도는 모습을 없앰
	FRotator SpawnRotation = FRotator::ZeroRotator;

	if (PlayerPawn)
	{
		SpawnRotation = (PlayerPawn->GetActorLocation() - AdjustedLocation).Rotation();
		SpawnRotation.Pitch = 0.0f;
		SpawnRotation.Roll = 0.0f;
	}

	//낸 몬스터를 등록하거나 스펙을 걸어주는 일은 하지 않음
	//게임모드가 액터 스폰을 지켜보고 있다가 알아서 등록하고 스펙은 몬스터가 스스로 MonsterInit에서 검
	return GetWorld()->SpawnActor<AMonsterBase>(ChosenClass, AdjustedLocation, SpawnRotation, SpawnParameters);
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
	if (!PlayerPawn) return 0;
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

// 웨이브 스폰
// 게임모드가 "이번 웨이브에 몇 마리 내라"고 시키면 여기서 간격을 두고 내보냄
// 실제로 한 마리를 만드는 일은 ExecuteSpawnActor가 하므로 그쪽만 고치면 스폰 방식이 바뀜

//게임모드가 주기마다 부르는 스폰 신호
//한 마리씩 내는 절차는 웨이브와 같으므로 SpawnWave를 그대로 씀
//간격을 주기의 절반으로 나눈 이유 다음 신호가 오기 전에 이번 몫을 다 내보내야 밀리지 않음
void AMonsterSpawnVolume::SpawnTick()
{
	if (MonstersPerSpawnTick <= 0)
	{
		return;
	}

	SpawnWave(MonstersPerSpawnTick, SpawnTickGap);
}

//웨이브 시작 남은 수를 더하고 타이머를 깨움
void AMonsterSpawnVolume::SpawnWave(int32 MonsterCount, float SpawnInterval)
{
	if (MonsterCount <= 0)
	{
		return;
	}

	//앞 웨이브가 아직 다 안 나왔으면 남은 수에 더함 타이머가 이어서 마저 내보냄
	PendingSpawnCount += MonsterCount;

	//이미 돌고 있으면 그대로 두고 남은 수만 늘어남 타이머를 다시 걸면 간격이 흐트러짐
	if (GetWorldTimerManager().IsTimerActive(WaveSpawnTimerHandle))
	{
		return;
	}

	//0 이하가 들어오면 타이머가 매 프레임 돌아서 최소값으로 막음
	const float SafeInterval = FMath::Max(SpawnInterval, 0.05f);

	//첫 마리도 간격만큼 기다렸다 나옴 웨이브 시작과 동시에 눈앞에 튀어나오지 않게
	GetWorldTimerManager().SetTimer(WaveSpawnTimerHandle, this, &AMonsterSpawnVolume::SpawnOneFromWave, SafeInterval, true);
}

//타이머가 돌 때마다 한 마리 내보냄
void AMonsterSpawnVolume::SpawnOneFromWave()
{
	if (PendingSpawnCount <= 0)
	{
		GetWorldTimerManager().ClearTimer(WaveSpawnTimerHandle);
		return;
	}

	PendingSpawnCount--;

	ExecuteSpawnActor();

	//마지막 한 마리를 냈으면 더 돌 이유가 없음
	if (PendingSpawnCount <= 0)
	{
		GetWorldTimerManager().ClearTimer(WaveSpawnTimerHandle);
	}
}
