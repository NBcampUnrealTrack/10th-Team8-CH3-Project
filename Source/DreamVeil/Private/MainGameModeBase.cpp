#include "MainGameModeBase.h"
#include "CombatStatsComponent.h"
#include "DreamVeilGameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "MonsterBase.h"
#include "MonsterSpawnVolume.h"
#include "TimerManager.h"

//보스 몬스터에 붙이는 액터 태그
//보스 클래스가 아직 없어서 태그로 구분함 보스 블루프린트의 Class Defaults > Actor > Tags에 Boss를 넣을 것
const FName BOSS_TAG = TEXT("Boss");

//레벨이 시작될 때
void AMainGameModeBase::BeginPlay()
{
	Super::BeginPlay();
	UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>();
	if (!DreamVeilGameInstance || !DreamVeilGameInstance->IsInLevelMap()) return;

	//기존 UI가 보는 제한 시간 타이머를 그대로 사용하므로 잠식 진행률의 의미가 바뀌지 않음
	LevelTimeLimit = FMath::Max(LevelTimeLimit, 0.1f);
	GetWorldTimerManager().SetTimer(LevelTimerHandle, this, &AMainGameModeBase::FailLevel, LevelTimeLimit, false);
	for (TActorIterator<AMonsterBase> MonsterIterator(GetWorld()); MonsterIterator; ++MonsterIterator)
		RegisterMonster(*MonsterIterator);
	ActorSpawnedHandle = GetWorld()->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &AMainGameModeBase::HandleActorSpawned));
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (UCombatStatsComponent* PlayerStats = PlayerPawn->FindComponentByClass<UCombatStatsComponent>())
			PlayerStats->OnDead.AddUniqueDynamic(this, &AMainGameModeBase::HandlePlayerDead);
	}
	//보스 예약은 일반 몬스터의 처치나 생성 완료 여부와 무관하게 레벨 시작 시각부터 셈
	GetWorldTimerManager().SetTimer(BossTimerHandle, this, &AMainGameModeBase::HandleBossTimeReached,
		FMath::Max(BossSpawnTime, 0.1f), false);
	StartNextWave();
}

// 웨이브
// 30초마다 한 번씩 스폰 볼륨들에게 몬스터를 내라고 시킴
// 마지막 웨이브를 내면 bAllMonstersSpawned를 켜서 기존 클리어 판정(TryClearLevel)이 그대로 동작함

//다음 웨이브를 내보냄
void AMainGameModeBase::StartNextWave()
{
	if (!LevelTimerHandle.IsValid() || bBossSpawned || bAllMonstersSpawned) return;
	if (IsPlayerDead()) { StopLevel(); return; }
	const float ElapsedTime = FMath::Max(0.0f, GetWorldTimerManager().GetTimerElapsed(LevelTimerHandle));
	if (ElapsedTime >= FMath::Max(BossSpawnTime, 0.1f)) { HandleBossTimeReached(); return; }

	//기존 HUD의 번호와 이벤트는 보존함. WaveCount는 표시 상한이며 소환 종료 조건으로 사용하지 않음
	CurrentWave = FMath::Clamp(FMath::FloorToInt(ElapsedTime / FMath::Max(WaveInterval, 0.1f)) + 1,
		1, FMath::Max(WaveCount, 1));
	RequestWaveSpawn();
	OnWaveChanged.Broadcast(CurrentWave, GetWaveCount());

	if (!GetWorldTimerManager().IsTimerActive(WaveTimerHandle))
	{
		//프레임 지연 뒤 같은 시각의 증가 요청을 여러 번 쌓지 않도록 한 프레임에는 한 번만 처리함
		FTimerManagerTimerParameters Parameters;
		Parameters.bLoop = true;
		Parameters.bMaxOncePerFrame = true;
		GetWorldTimerManager().SetTimer(WaveTimerHandle, this, &AMainGameModeBase::StartNextWave,
			FMath::Max(WaveInterval, 0.1f), Parameters);
	}
}

//맵에 있는 스폰 볼륨 전부에게 이번 웨이브 몬스터를 내라고 시킴
void AMainGameModeBase::RequestWaveSpawn()
{
	if (!LevelTimerHandle.IsValid() || bBossSpawned || bAllMonstersSpawned) return;
	const float ElapsedTime = FMath::Max(0.0f, GetWorldTimerManager().GetTimerElapsed(LevelTimerHandle));
	//콜백 순서에 관계없이 보스 시점부터 일반 소환 예약을 멈춤
	if (ElapsedTime >= FMath::Max(BossSpawnTime, 0.1f)) { HandleBossTimeReached(); return; }
	//기존 MonstersPerWave와 WaveInterval을 재사용함. 현재 수량은 경과 시간으로 계산하므로 별도 누적값이 필요 없음
	const int64 Count = FMath::Max(MonstersPerWave, 0)
		+ static_cast<int64>(FMath::FloorToInt(ElapsedTime / FMath::Max(WaveInterval, 0.1f)))
		* FMath::Max(MonsterCountIncrease, 0);
	PendingSpawnCount = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(PendingSpawnCount) + Count, MAX_int32));
	if (PendingSpawnCount > 0 && !GetWorldTimerManager().IsTimerActive(MonsterSpawnTimerHandle))
		GetWorldTimerManager().SetTimer(MonsterSpawnTimerHandle, this, &AMainGameModeBase::SpawnNextMonster,
			FMath::Max(WaveSpawnInterval, 0.05f), false);
}

//지금 몇 번째 웨이브인지
int32 AMainGameModeBase::GetCurrentWave() const
{
	return CurrentWave;
}

//전체 웨이브 수
int32 AMainGameModeBase::GetWaveCount() const
{
	return FMath::Max(WaveCount, 1);
}

//지금 맵이 마지막 레벨인지
bool AMainGameModeBase::IsFinalLevel() const
{
	const UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>();

	if (!DreamVeilGameInstance)
	{
		return false;
	}

	//레벨 번호가 0이면 로비나 메인 메뉴라 마지막 레벨이 아님
	const int32 LevelNumber = DreamVeilGameInstance->GetCurrentLevelNumber();

	return LevelNumber > 0 && LevelNumber >= DreamVeilGameInstance->GetLevelCount();
}

//보스에 도전
void AMainGameModeBase::AcceptBossChallenge()
{
	//기존 UI 버튼 노드는 그대로 연결됨. 자동 등장 시점에만 진입을 허용하여 중복/조기 생성을 막음
	if (!LevelTimerHandle.IsValid() || !bBossChoicePending || bBossSpawned) return;
	if (IsPlayerDead()) { StopLevel(); return; }
	bBossChoicePending = false;
	bBossSpawned = true;
	GetWorldTimerManager().ClearTimer(BossTimerHandle);
	CancelMonsterSpawning();
	SpawnBoss();
}

//보스를 넘기고 로비로
void AMainGameModeBase::DeclineBossChallenge()
{
	//기존 버튼 노드를 보존함. 자동 보스전에서는 선택 대기 상태가 없으므로 보스 처치를 건너뛰지 못함
	if (!LevelTimerHandle.IsValid() || !bBossChoicePending || bBossSpawned) return;
	bBossChoicePending = false;
	ClearLevel();
}

//보스를 냄
void AMainGameModeBase::SpawnBoss()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!BossClass || !PlayerPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("Boss spawn requires BossClass and a player pawn."));
		StopLevel();
		return;
	}
	const FTransform SpawnTransform(PlayerPawn->GetActorRotation(),
		PlayerPawn->GetActorLocation() + PlayerPawn->GetActorForwardVector() * BossSpawnDistance);
	//기존 보스 위치/충돌 정책은 유지하고 BeginPlay 전에 Boss 태그와 사망 이벤트를 연결함
	AMonsterBase* Boss = GetWorld()->SpawnActorDeferred<AMonsterBase>(BossClass, SpawnTransform,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Boss)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to spawn boss."));
		StopLevel();
		return;
	}
	Boss->Tags.AddUnique(BOSS_TAG);
	if (UCombatStatsComponent* BossStats = Boss->FindComponentByClass<UCombatStatsComponent>())
		BossStats->OnDead.AddUniqueDynamic(this, &AMainGameModeBase::HandleBossDead);
	UGameplayStatics::FinishSpawningActor(Boss, SpawnTransform);
	if (IsValid(Boss))
	{
		//BP 생성 스크립트가 컴포넌트를 바꿔도 최종 컴포넌트에 한 번만 연결되게 보강함
		if (UCombatStatsComponent* BossStats = Boss->FindComponentByClass<UCombatStatsComponent>())
			BossStats->OnDead.AddUniqueDynamic(this, &AMainGameModeBase::HandleBossDead);
		if (!Boss->GetController()) Boss->SpawnDefaultController();
	}
	else if (LevelTimerHandle.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Boss was removed during construction."));
		StopLevel();
	}
}

//스포너가 이번 레벨에 낼 몬스터를 다 냈을 때
void AMainGameModeBase::NotifyAllMonstersSpawned()
{
	//기존 스포너/BP의 완료 알림은 계속 받지만 한 묶음 완료만으로 레벨이 조기 종료되지는 않음
	if (!LevelTimerHandle.IsValid()) return;
	if (!BossClass && GetWorldTimerManager().GetTimerElapsed(LevelTimerHandle) >= FMath::Max(BossSpawnTime, 0.1f))
		bAllMonstersSpawned = true;
	TryClearLevel();
}

//몬스터의 사망 이벤트를 구독하고 살아있는 수에 더함
void AMainGameModeBase::RegisterMonster(AMonsterBase* Monster)
{
	if (!Monster)
	{
		return;
	}

	//몬스터 헤더의 변수 이름에 기대지 않고 컴포넌트로 찾음 몬스터 쪽 코드가 바뀌어도 여기는 안 고쳐도 됨
	UCombatStatsComponent* MonsterStats = Monster->FindComponentByClass<UCombatStatsComponent>();

	if (!MonsterStats || MonsterStats->IsDead()
		|| MonsterStats->OnDead.IsAlreadyBound(this, &AMainGameModeBase::HandleMonsterDead))
	{
		return;
	}

	AliveMonsterCount++;

	//배치된 몬스터와 생성 이벤트가 중복으로 들어와도 한 번만 집계함
	MonsterStats->OnDead.AddUniqueDynamic(this, &AMainGameModeBase::HandleMonsterDead);

	//보스는 죽는 순간 따로 클리어 처리 L4는 보스를 잡아야 Endless가 열림
	//살아있는 수에서도 빠져야 하므로 위의 HandleMonsterDead 구독은 그대로 둠
	if (IsBossMonster(Monster))
	{
		MonsterStats->OnDead.AddUniqueDynamic(this, &AMainGameModeBase::HandleBossDead);
	}
}

//월드에 액터가 스폰될 때마다 불림
void AMainGameModeBase::HandleActorSpawned(AActor* SpawnedActor)
{
	//몬스터가 아니면 Cast가 nullptr를 돌려주고 RegisterMonster가 바로 돌아감
	RegisterMonster(Cast<AMonsterBase>(SpawnedActor));
}

//등록한 몬스터가 죽었을 때
void AMainGameModeBase::HandleMonsterDead()
{
	AliveMonsterCount = FMath::Max(AliveMonsterCount - 1, 0);

	TryClearLevel();
}

//보스가 죽었을 때
void AMainGameModeBase::HandleBossDead()
{
	//늦게 전달된 사망 알림이나 자동 등장 이전의 보스 태그 액터로 레벨이 끝나지 않게 함
	if (bBossSpawned) ClearLevel();
}

//스포너가 다 냈고 살아있는 몬스터가 없으면 클리어
void AMainGameModeBase::TryClearLevel()
{
	//보스 클래스가 있으면 보스 사망만 클리어 조건임. 일반 몬스터 전멸은 시간 흐름을 건너뛰지 않음
	if (!LevelTimerHandle.IsValid() || BossClass || bBossSpawned || !bAllMonstersSpawned
		|| AliveMonsterCount > 0 || PendingSpawnCount > 0) return;
	//보스 미지정 맵은 기존 전멸 클리어를 유지하되 구형 SpawnWave 예약까지 모두 끝났는지 확인함
	for (TActorIterator<AMonsterSpawnVolume> VolumeIterator(GetWorld()); VolumeIterator; ++VolumeIterator)
		if (VolumeIterator->HasPendingSpawns()) return;
	ClearLevel();
}

//레벨 타이머를 멈춤
bool AMainGameModeBase::StopLevel()
{
	if (!LevelTimerHandle.IsValid()) return false;
	GetWorldTimerManager().ClearTimer(LevelTimerHandle);
	GetWorldTimerManager().ClearTimer(BossTimerHandle);
	bBossChoicePending = false;
	CancelMonsterSpawning();
	return true;
}

//플레이어가 죽었는지
bool AMainGameModeBase::IsPlayerDead() const
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	UCombatStatsComponent* PlayerStats = PlayerPawn ? PlayerPawn->FindComponentByClass<UCombatStatsComponent>() : nullptr;

	return PlayerStats && PlayerStats->IsDead();
}

//클리어 다음 레벨이 열림
void AMainGameModeBase::ClearLevel()
{
	if (!StopLevel())
	{
		return;
	}

	//타이머를 먼저 멈추고 확인하는 이유 죽은 뒤에 화염 데미지로 마지막 몬스터가 죽어도 클리어되지 않게
	if (IsPlayerDead())
	{
		return;
	}

	UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>();

	if (!DreamVeilGameInstance)
	{
		return;
	}

	//증강 저장 진행도 올리기 로비 이동은 GameInstance가 한 번에 함
	DreamVeilGameInstance->CompleteCurrentLevel();
}

//제한 시간 초과
void AMainGameModeBase::FailLevel()
{
	if (!StopLevel())
	{
		return;
	}

	//죽은 뒤에 시간이 끝났으면 로비로 보내지 않음 게임 오버 쪽 흐름과 겹치지 않게
	if (IsPlayerDead())
	{
		return;
	}

	UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>();

	if (!DreamVeilGameInstance)
	{
		return;
	}

	DreamVeilGameInstance->FailCurrentLevel();
}

//보스 몬스터인지
bool AMainGameModeBase::IsBossMonster(const AActor* Actor)
{
	return Actor && Actor->ActorHasTag(BOSS_TAG);
}

float AMainGameModeBase::GetLevelTimeRemaining() const
{
	// 타이머가 없으면 0
	if (!LevelTimerHandle.IsValid())
	{
		return 0.0f;
	}
	// 현재 남은 시간
	return GetWorldTimerManager().GetTimerRemaining(LevelTimerHandle);
}
float AMainGameModeBase::GetLevelTimeLimit() const
{
	// 전체 제한 시간
	return LevelTimeLimit;
}
float AMainGameModeBase::GetLevelTimeProgress() const
{
	// 잘못된 값 방지
	if (LevelTimeLimit <= 0.0f)
	{
		return 0.0f;
	}
	const float RemainingTime = GetLevelTimeRemaining();
	// 시간이 줄수록 0 → 1
	return FMath::Clamp(
		1.0f - (RemainingTime / LevelTimeLimit),
		0.0f,
		1.0f
	);
}

//기존 함수/프로퍼티는 유지하고 시간 기반 진행에 필요한 내부 처리만 추가함
void AMainGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(LevelTimerHandle);
	GetWorldTimerManager().ClearTimer(BossTimerHandle);
	CancelMonsterSpawning();
	GetWorld()->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	Super::EndPlay(EndPlayReason);
}

void AMainGameModeBase::HandleBossTimeReached()
{
	if (!LevelTimerHandle.IsValid() || bBossSpawned || bAllMonstersSpawned) return;
	if (IsPlayerDead()) { StopLevel(); return; }
	GetWorldTimerManager().ClearTimer(BossTimerHandle);
	GetWorldTimerManager().ClearTimer(WaveTimerHandle);
	//기존 HUD는 마지막 번호를 표시하도록 유지하지만 도전 선택 UI는 자동으로 띄우지 않음
	CurrentWave = GetWaveCount();
	OnWaveChanged.Broadcast(CurrentWave, GetWaveCount());
	if (BossClass)
	{
		bBossChoicePending = true;
		AcceptBossChallenge();
	}
	else
	{
		//보스 없는 기존 맵도 진행 가능하게 생성 중인 예약과 남은 몬스터를 다 처리한 뒤 클리어함
		bAllMonstersSpawned = true;
		TryClearLevel();
	}
}

void AMainGameModeBase::SpawnNextMonster()
{
	if (!LevelTimerHandle.IsValid() || bBossSpawned) return;
	if (IsPlayerDead()) { StopLevel(); return; }
	if (!bAllMonstersSpawned
		&& GetWorldTimerManager().GetTimerElapsed(LevelTimerHandle) >= FMath::Max(BossSpawnTime, 0.1f))
	{
		HandleBossTimeReached();
		if (!LevelTimerHandle.IsValid() || bBossSpawned) return;
	}
	//볼륨마다 같은 수량을 뿌리지 않고 전체에서 한 볼륨에 한 마리만 요청하므로 동시에 생성되지 않음
	TArray<AActor*> Volumes;
	UGameplayStatics::GetAllActorsOfClass(this, AMonsterSpawnVolume::StaticClass(), Volumes);
	if (PendingSpawnCount > 0 && !Volumes.IsEmpty())
	{
		AMonsterSpawnVolume* Volume = CastChecked<AMonsterSpawnVolume>(Volumes[FMath::RandRange(0, Volumes.Num() - 1)]);
		if (Volume->TrySpawnMonster()) --PendingSpawnCount;
		//공간이나 NavMesh가 없어 실패한 경우 예약 수량은 유지하고 다음 간격에 다시 시도함
	}
	if (LevelTimerHandle.IsValid() && !bBossSpawned && PendingSpawnCount > 0)
	{
		//한 번짜리 타이머를 다시 예약하여 프레임이 지연돼도 밀린 몬스터를 한꺼번에 내보내지 않음
		GetWorldTimerManager().SetTimer(MonsterSpawnTimerHandle, this, &AMainGameModeBase::SpawnNextMonster,
			FMath::Max(WaveSpawnInterval, 0.05f), false);
	}
	else
	{
		TryClearLevel();
	}
}

void AMainGameModeBase::CancelMonsterSpawning()
{
	GetWorldTimerManager().ClearTimer(WaveTimerHandle);
	GetWorldTimerManager().ClearTimer(MonsterSpawnTimerHandle);
	PendingSpawnCount = 0;
	for (TActorIterator<AMonsterSpawnVolume> VolumeIterator(GetWorld()); VolumeIterator; ++VolumeIterator)
		VolumeIterator->CancelPendingSpawns();
}

void AMainGameModeBase::HandlePlayerDead()
{
	StopLevel();
}


//이전 동작 학습 메모: 원문 주석은 보존하며 현재 시간 기반 동작과는 구분함
//로비와 메인 메뉴도 이 게임모드를 쓰므로 L1~L4가 아니면 제한 시간을 걸지 않음
//여기서 거르지 않으면 로비에서도 시간이 다 되면 클리어 처리돼서 진행도가 올라감
//제한 시간이 끝날 때까지 못 깨면 실패
//맵에 미리 배치해둔 몬스터 등록
//앞으로 스폰될 몬스터도 등록 스포너 코드를 고치지 않고 게임모드가 알아서 셀 수 있게 월드의 스폰 알림을 받음
//핸들러를 따로 해제하지 않는 이유 레벨이 바뀌면 월드와 함께 사라지고 CreateUObject라 게임모드가 먼저 사라져도 안전함
//첫 웨이브는 레벨이 시작되자마자 바로 내보냄
//마지막 웨이브를 냈으면 더 낼 게 없다고 알림
//이걸 켜야 남은 몬스터를 다 잡았을 때 TryClearLevel이 클리어로 넘어감
//다음 웨이브 예약 이미 돌고 있으면 다시 걸지 않아도 되지만
//첫 웨이브는 타이머 없이 들어오므로 여기서 한 번 걸어둠
//볼륨을 미리 모아두지 않고 매번 찾는 이유
//웨이브 도중에 볼륨이 생기거나 사라져도 알아서 반영되고 목록을 관리할 필요가 없음
//선택 창이 떠 있지 않은데 불리면 무시 버튼을 두 번 눌러도 보스가 둘 나오지 않음
//보스전에도 잡몹이 계속 나오게 웨이브를 다시 돌림
//웨이브 수를 세는 CurrentWave는 그대로 둬서 HUD에는 마지막 웨이브로 표시됨
//플레이어 위치를 기준으로 앞쪽에 냄 보스 전용 스폰 지점이 생기면 그쪽으로 바꿀 것
//바닥이나 벽에 조금 겹쳐도 일단 나오게 함 안 그러면 보스가 안 나와서 레벨이 끝나지 않음
//보스 판정은 액터 태그로 하므로 블루프린트에서 태그를 빠뜨렸어도 여기서 달아줌
//이게 없으면 보스를 잡아도 클리어가 안 됨
//태그가 없는 채로 등록됐으니 보스 사망 구독을 여기서 걸어줌
//마지막 몬스터가 알림보다 먼저 죽었을 수도 있으니 바로 한 번 확인
//아직 더 나올 몬스터가 있으면 0마리여도 클리어가 아님 스폰 사이 빈틈에 레벨이 끝나버리는 걸 막음
//마지막 레벨에서 보스를 아직 안 냈으면 바로 끝내지 않고 도전할지 물음
//BossClass를 안 넣었으면 물을 것이 없으므로 평소처럼 클리어됨
//고르는 동안 시간이 흘러 실패 처리되면 안 되므로 제한 시간을 멈춤
//ClearTimer가 아니라 PauseTimer인 이유 핸들이 살아 있어야 나중에 StopLevel이 동작함
//잡몹도 그만 나오게 웨이브를 멈춤 도전을 고르면 AcceptBossChallenge가 다시 켬
//타이머를 건 적이 없는 맵(로비 메인 메뉴)이거나 이미 끝낸 레벨이면 false
//시간 초과와 마지막 처치가 같은 프레임에 겹쳐도 레벨이 두 번 끝나지 않게 막음
//ClearTimer가 핸들을 무효로 만들어서 위 검사가 다음 호출을 막아줌 타이머 콜백 안에서 불러도 안전함
