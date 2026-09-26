// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterSpawnVolume.h"
#include "Kismet/GameplayStatics.h"

#include "MainPlayerCharacter.h"
#include "TimerManager.h"
#include "DreamVeilGameInstance.h"
#include "MainGameModeBase.h"
#include "MonsterBase.h"
#include "Components/CapsuleComponent.h"

AMonsterSpawnVolume::AMonsterSpawnVolume()
{
	MaxDifficultyLevel = 30;
	EliteMonsterMinRate = 5.0f;
	EliteMonsterMaxRate = 50.0f;
	DifficultyCurve = 1.0f;
}

void AMonsterSpawnVolume::ExecuteSpawnActor()
{
	//기존 이벤트 함수의 이름과 반환형을 유지하면서 실제 생성 처리는 한 곳에서 재사용함
	TrySpawnMonster();
}

bool AMonsterSpawnVolume::TrySpawnMonster()
{
	if (const AMainGameModeBase* Mode = GetWorld()->GetAuthGameMode<AMainGameModeBase>())
		if (!Mode->CanSpawnMonsters()) return false;
	FVector SpawnLocation;
	if (!TryGetRandomNavLocation(SpawnLocation)) return false;
	//엘리트 스폰
	TSubclassOf<AMonsterBase> MonsterClass;
	if (FMath::FRandRange(0.0f, 100.0f) < GetEliteRate()) MonsterClass = SelectMonsterClass(true);
	//일반 몬스터 스폰
	//정예 목록을 아직 지정하지 않았으면 기존 일반 목록으로 생성함
	if (!MonsterClass) MonsterClass = SelectMonsterClass(false);
	if (!MonsterClass) return false;
	//NavMesh 좌표는 바닥이므로 캡슐 반높이만큼 올려 지면에 파묻히지 않게 함
	SpawnLocation.Z += MonsterClass->GetDefaultObject<AMonsterBase>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FActorSpawnParameters Parameters;
	//0.5초 간격만으로 공간 충돌은 보장되지 않으므로 막힌 위치에서는 생성하지 않고 재시도함
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	AMonsterBase* Monster = GetWorld()->SpawnActor<AMonsterBase>(MonsterClass, SpawnLocation, FRotator::ZeroRotator, Parameters);
	if (!IsValid(Monster)) return false;
	if (!Monster->GetController()) Monster->SpawnDefaultController();
	return true;
}

TSubclassOf<AMonsterBase> AMonsterSpawnVolume::SelectMonsterClass(bool bElite) const
{
	const TArray<TSubclassOf<AMonsterBase>>& Classes = bElite ? EliteMonsterClasses : BaseMonsterClasses;
	TArray<TSubclassOf<AMonsterBase>> Candidates;
	for (const TSubclassOf<AMonsterBase>& Class : Classes)
		if (Class && !Class->HasAnyClassFlags(CLASS_Abstract)) Candidates.Add(Class);
	if (Candidates.IsEmpty())
	{
		//기존 액터 참조를 지우거나 타입을 바꾸지 않고 그 액터의 클래스를 얻어 새 액터를 생성함
		const TArray<TObjectPtr<AMonsterBase>>& Monsters = bElite ? EliteMonsters : BaseMonsters;
		for (const AMonsterBase* Monster : Monsters)
			if (IsValid(Monster)) Candidates.Add(Monster->GetClass());
	}
	return Candidates.IsEmpty() ? nullptr : Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
}

void AMonsterSpawnVolume::CancelPendingSpawns()
{
	GetWorldTimerManager().ClearTimer(WaveSpawnTimerHandle);
	PendingSpawnCount = 0;
}

void AMonsterSpawnVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelPendingSpawns();
	Super::EndPlay(EndPlayReason);
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
	//생성 순서나 폰 교체로 참조가 사라졌으면 다시 찾음. 기존 변수 타입은 유지함
	if (!IsValid(PlayerPawn)) PlayerPawn = Cast<AMainPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!IsValid(PlayerPawn)) return 0;
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

//웨이브 시작 남은 수를 더하고 타이머를 깨움
void AMonsterSpawnVolume::SpawnWave(int32 MonsterCount, float SpawnInterval)
{
	if (const AMainGameModeBase* Mode = GetWorld()->GetAuthGameMode<AMainGameModeBase>())
	{
		if (!Mode->CanSpawnMonsters()) { CancelPendingSpawns(); return; }
	}
	if (MonsterCount <= 0)
	{
		return;
	}

	//앞 웨이브가 아직 다 안 나왔으면 남은 수에 더함 타이머가 이어서 마저 내보냄
	PendingSpawnCount = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(PendingSpawnCount) + MonsterCount, MAX_int32));

	//이미 돌고 있으면 그대로 두고 남은 수만 늘어남 타이머를 다시 걸면 간격이 흐트러짐
	if (GetWorldTimerManager().IsTimerActive(WaveSpawnTimerHandle))
	{
		return;
	}

	//0 이하가 들어오면 타이머가 매 프레임 돌아서 최소값으로 막음
	const float SafeInterval = FMath::Max(SpawnInterval, 0.05f);

	//첫 마리도 간격만큼 기다렸다 나옴 웨이브 시작과 동시에 눈앞에 튀어나오지 않게
	//구형 BP가 직접 호출하더라도 지연된 반복 타이머가 한 프레임에 여러 마리를 만들지 않게 함
	FTimerManagerTimerParameters Parameters;
	Parameters.bLoop = true;
	Parameters.bMaxOncePerFrame = true;
	GetWorldTimerManager().SetTimer(WaveSpawnTimerHandle, this, &AMonsterSpawnVolume::SpawnOneFromWave, SafeInterval, Parameters);
}

//타이머가 돌 때마다 한 마리 내보냄
void AMonsterSpawnVolume::SpawnOneFromWave()
{
	if (PendingSpawnCount <= 0)
	{
		GetWorldTimerManager().ClearTimer(WaveSpawnTimerHandle);
		return;
	}

	//실제로 생성된 경우에만 차감하여 NavMesh/충돌 실패로 요청 수량이 사라지는 것을 막음
	if (TrySpawnMonster()) --PendingSpawnCount;

	//마지막 한 마리를 냈으면 더 돌 이유가 없음
	if (PendingSpawnCount <= 0)
	{
		GetWorldTimerManager().ClearTimer(WaveSpawnTimerHandle);
		//기존 완료 알림을 재사용함. 게임모드가 시간과 다른 예약까지 확인한 뒤 클리어 여부를 정함
		if (AMainGameModeBase* Mode = GetWorld()->GetAuthGameMode<AMainGameModeBase>()) Mode->NotifyAllMonstersSpawned();
	}
}
