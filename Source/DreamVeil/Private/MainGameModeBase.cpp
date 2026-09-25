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

	//로비와 메인 메뉴도 이 게임모드를 쓰므로 L1~L4가 아니면 제한 시간을 걸지 않음
	//여기서 거르지 않으면 로비에서도 시간이 다 되면 클리어 처리돼서 진행도가 올라감
	UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>();

	if (!DreamVeilGameInstance || !DreamVeilGameInstance->IsInLevelMap())
	{
		return;
	}

	//제한 시간이 끝날 때까지 못 깨면 실패
	GetWorldTimerManager().SetTimer(LevelTimerHandle, this, &AMainGameModeBase::FailLevel, LevelTimeLimit, false);

	//맵에 미리 배치해둔 몬스터 등록
	for (TActorIterator<AMonsterBase> MonsterIterator(GetWorld()); MonsterIterator; ++MonsterIterator)
	{
		RegisterMonster(*MonsterIterator);
	}

	//앞으로 스폰될 몬스터도 등록 스포너 코드를 고치지 않고 게임모드가 알아서 셀 수 있게 월드의 스폰 알림을 받음
	//핸들러를 따로 해제하지 않는 이유 레벨이 바뀌면 월드와 함께 사라지고 CreateUObject라 게임모드가 먼저 사라져도 안전함
	GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &AMainGameModeBase::HandleActorSpawned));

	//첫 웨이브는 레벨이 시작되자마자 바로 내보냄
	StartNextWave();
}

// 웨이브
// 30초마다 한 번씩 스폰 볼륨들에게 몬스터를 내라고 시킴
// 마지막 웨이브를 내면 bAllMonstersSpawned를 켜서 기존 클리어 판정(TryClearLevel)이 그대로 동작함

//다음 웨이브를 내보냄
void AMainGameModeBase::StartNextWave()
{
	CurrentWave++;

	RequestWaveSpawn();

	OnWaveChanged.Broadcast(CurrentWave, WaveCount);

	//마지막 웨이브를 냈으면 더 낼 게 없다고 알림
	//이걸 켜야 남은 몬스터를 다 잡았을 때 TryClearLevel이 클리어로 넘어감
	if (CurrentWave >= WaveCount)
	{
		GetWorldTimerManager().ClearTimer(WaveTimerHandle);

		NotifyAllMonstersSpawned();

		return;
	}

	//다음 웨이브 예약 이미 돌고 있으면 다시 걸지 않아도 되지만
	//첫 웨이브는 타이머 없이 들어오므로 여기서 한 번 걸어둠
	if (!GetWorldTimerManager().IsTimerActive(WaveTimerHandle))
	{
		GetWorldTimerManager().SetTimer(WaveTimerHandle, this, &AMainGameModeBase::StartNextWave, WaveInterval, true);
	}
}

//맵에 있는 스폰 볼륨 전부에게 이번 웨이브 몬스터를 내라고 시킴
void AMainGameModeBase::RequestWaveSpawn()
{
	//볼륨을 미리 모아두지 않고 매번 찾는 이유
	//웨이브 도중에 볼륨이 생기거나 사라져도 알아서 반영되고 목록을 관리할 필요가 없음
	for (TActorIterator<AMonsterSpawnVolume> VolumeIterator(GetWorld()); VolumeIterator; ++VolumeIterator)
	{
		VolumeIterator->SpawnWave(MonstersPerWave, WaveSpawnInterval);
	}
}

//지금 몇 번째 웨이브인지
int32 AMainGameModeBase::GetCurrentWave() const
{
	return CurrentWave;
}

//전체 웨이브 수
int32 AMainGameModeBase::GetWaveCount() const
{
	return WaveCount;
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
	//선택 창이 떠 있지 않은데 불리면 무시 버튼을 두 번 눌러도 보스가 둘 나오지 않음
	if (!bBossChoicePending)
	{
		return;
	}

	bBossChoicePending = false;
	bBossSpawned = true;

	SpawnBoss();

	//보스전에도 잡몹이 계속 나오게 웨이브를 다시 돌림
	//웨이브 수를 세는 CurrentWave는 그대로 둬서 HUD에는 마지막 웨이브로 표시됨
	GetWorldTimerManager().SetTimer(WaveTimerHandle, this, &AMainGameModeBase::RequestWaveSpawn, WaveInterval, true);
}

//보스를 넘기고 로비로
void AMainGameModeBase::DeclineBossChallenge()
{
	if (!bBossChoicePending)
	{
		return;
	}

	bBossChoicePending = false;

	ClearLevel();
}

//보스를 냄
void AMainGameModeBase::SpawnBoss()
{
	if (!BossClass)
	{
		return;
	}

	//플레이어 위치를 기준으로 앞쪽에 냄 보스 전용 스폰 지점이 생기면 그쪽으로 바꿀 것
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	if (!PlayerPawn)
	{
		return;
	}

	const FVector SpawnLocation = PlayerPawn->GetActorLocation() + PlayerPawn->GetActorForwardVector() * BossSpawnDistance;

	FActorSpawnParameters SpawnParameters;

	//바닥이나 벽에 조금 겹쳐도 일단 나오게 함 안 그러면 보스가 안 나와서 레벨이 끝나지 않음
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AMonsterBase* Boss = GetWorld()->SpawnActor<AMonsterBase>(BossClass, SpawnLocation, PlayerPawn->GetActorRotation(), SpawnParameters);

	//보스 판정은 액터 태그로 하므로 블루프린트에서 태그를 빠뜨렸어도 여기서 달아줌
	//이게 없으면 보스를 잡아도 클리어가 안 됨
	if (Boss && !Boss->ActorHasTag(BOSS_TAG))
	{
		Boss->Tags.Add(BOSS_TAG);

		//태그가 없는 채로 등록됐으니 보스 사망 구독을 여기서 걸어줌
		if (UCombatStatsComponent* BossStats = Boss->FindComponentByClass<UCombatStatsComponent>())
		{
			BossStats->OnDead.AddDynamic(this, &AMainGameModeBase::HandleBossDead);
		}
	}
}

//스포너가 이번 레벨에 낼 몬스터를 다 냈을 때
void AMainGameModeBase::NotifyAllMonstersSpawned()
{
	bAllMonstersSpawned = true;

	//마지막 몬스터가 알림보다 먼저 죽었을 수도 있으니 바로 한 번 확인
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

	if (!MonsterStats || MonsterStats->IsDead())
	{
		return;
	}

	AliveMonsterCount++;

	MonsterStats->OnDead.AddDynamic(this, &AMainGameModeBase::HandleMonsterDead);

	//보스는 죽는 순간 따로 클리어 처리 L4는 보스를 잡아야 Endless가 열림
	//살아있는 수에서도 빠져야 하므로 위의 HandleMonsterDead 구독은 그대로 둠
	if (IsBossMonster(Monster))
	{
		MonsterStats->OnDead.AddDynamic(this, &AMainGameModeBase::HandleBossDead);
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
	ClearLevel();
}

//스포너가 다 냈고 살아있는 몬스터가 없으면 클리어
void AMainGameModeBase::TryClearLevel()
{
	//아직 더 나올 몬스터가 있으면 0마리여도 클리어가 아님 스폰 사이 빈틈에 레벨이 끝나버리는 걸 막음
	if (!bAllMonstersSpawned || AliveMonsterCount > 0)
	{
		return;
	}

	//마지막 레벨에서 보스를 아직 안 냈으면 바로 끝내지 않고 도전할지 물음
	//BossClass를 안 넣었으면 물을 것이 없으므로 평소처럼 클리어됨
	if (IsFinalLevel() && BossClass && !bBossSpawned && !bBossChoicePending)
	{
		bBossChoicePending = true;

		//고르는 동안 시간이 흘러 실패 처리되면 안 되므로 제한 시간을 멈춤
		//ClearTimer가 아니라 PauseTimer인 이유 핸들이 살아 있어야 나중에 StopLevel이 동작함
		GetWorldTimerManager().PauseTimer(LevelTimerHandle);

		//잡몹도 그만 나오게 웨이브를 멈춤 도전을 고르면 AcceptBossChallenge가 다시 켬
		GetWorldTimerManager().ClearTimer(WaveTimerHandle);

		OnBossChoiceReady.Broadcast();

		return;
	}

	ClearLevel();
}

//레벨 타이머를 멈춤
bool AMainGameModeBase::StopLevel()
{
	//타이머를 건 적이 없는 맵(로비 메인 메뉴)이거나 이미 끝낸 레벨이면 false
	//시간 초과와 마지막 처치가 같은 프레임에 겹쳐도 레벨이 두 번 끝나지 않게 막음
	if (!LevelTimerHandle.IsValid())
	{
		return false;
	}

	//ClearTimer가 핸들을 무효로 만들어서 위 검사가 다음 호출을 막아줌 타이머 콜백 안에서 불러도 안전함
	GetWorldTimerManager().ClearTimer(LevelTimerHandle);

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
