#include "MainGameModeBase.h"

#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "MainPlayerController.h"
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

	if (!DreamVeilGameInstance)
	{
		return;
	}

	//무한 모드는 제한 시간도 웨이브도 없어서 완전히 다른 흐름을 탐
	//아래 레벨용 준비를 같이 하면 시간이 다 됐을 때 실패 처리가 돌아서 무한 모드가 끝나버림
	//맵에 맞는 배경음부터 틀어둠 로비 메인메뉴 레벨 Endless가 각각 다름
	//아래 갈래마다 따로 틀지 않는 이유 로비와 메인메뉴는 중간에 return으로 빠져나감
	PlayBGM(GetBaseBGM());

	if (DreamVeilGameInstance->IsInEndless())
	{
		StartEndlessMode();
		return;
	}

	//로비와 메인 메뉴도 이 게임모드를 쓰므로 L1~L4가 아니면 아무것도 하지 않음
	if (!DreamVeilGameInstance->IsInLevelMap())
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

	//첫 웨이브 시작 웨이브는 이제 몰아내기가 아니라 시간 눈금이라 스폰은 아래 타이머가 맡음
	StartNextWave();

	//주기마다 스폰 볼륨에게 내라고 신호 첫 신호도 주기만큼 기다렸다 나감
	//레벨 시작과 동시에 눈앞에 몬스터가 튀어나오지 않게 하려는 것
	GetWorldTimerManager().SetTimer(ContinuousSpawnTimerHandle, this, &AMainGameModeBase::RequestContinuousSpawn, FMath::Max(ContinuousSpawnInterval, 0.1f), true);

	//제한 시간과 별개로 잠식도도 차오르기 시작함
	//제한 시간은 "다 못 깼다" 판정이고 잠식도는 "버티지 못했다" 판정으로 서로 독립임
	StartCorruption();
}

// 웨이브
// 30초마다 한 번씩 스폰 볼륨들에게 몬스터를 내라고 시킴
// 마지막 웨이브를 내면 bAllMonstersSpawned를 켜서 기존 클리어 판정(TryClearLevel)이 그대로 동작함

//다음 웨이브를 내보냄
void AMainGameModeBase::StartNextWave()
{
	CurrentWave++;

	//마지막 웨이브까지 다 지났으면 이제 그만 냄
	//웨이브가 끝나는 순간이 아니라 마지막 웨이브가 제 시간을 다 쓴 뒤에 멈춰야
	//6웨이브에도 몬스터가 계속 나오다가 끊김 여기서 바로 멈추면 6웨이브가 텅 빈 채로 끝남
	if (CurrentWave > WaveCount)
	{
		GetWorldTimerManager().ClearTimer(WaveTimerHandle);

		//스폰 신호도 멈춰야 살아있는 수가 0으로 떨어질 수 있음
		//이걸 안 멈추면 3초마다 새 몬스터가 나와서 영원히 클리어되지 않음
		GetWorldTimerManager().ClearTimer(ContinuousSpawnTimerHandle);

		NotifyAllMonstersSpawned();

		return;
	}

	OnWaveChanged.Broadcast(CurrentWave, WaveCount);

	//마지막 웨이브에 보스 등장 레벨마다 하나씩 나옴
	//선택창을 띄우지 않는 이유 보스를 잡는 것이 곧 해금 조건이라 건너뛸 수 있으면 조건이 성립하지 않음
	if (CurrentWave >= WaveCount && !bBossSpawned)
	{
		bBossSpawned = true;

		SpawnBoss();
	}

	//다음 웨이브 예약 이미 돌고 있으면 다시 걸지 않아도 되지만
	//첫 웨이브는 타이머 없이 들어오므로 여기서 한 번 걸어둠
	if (!GetWorldTimerManager().IsTimerActive(WaveTimerHandle))
	{
		GetWorldTimerManager().SetTimer(WaveTimerHandle, this, &AMainGameModeBase::StartNextWave, WaveInterval, true);
	}
}

//맵에 있는 스폰 볼륨 전부에게 이번 웨이브 몬스터를 내라고 시킴
void AMainGameModeBase::RequestContinuousSpawn()
{
	//볼륨을 미리 모아두지 않고 매번 찾는 이유
	//도중에 볼륨이 생기거나 사라져도 알아서 반영되고 목록을 관리할 필요가 없음
	//몇 마리를 낼지 여기서 정하지 않는 이유 좁은 방과 넓은 마당의 적정 밀도가 다름 볼륨이 스스로 정함
	for (TActorIterator<AMonsterSpawnVolume> VolumeIterator(GetWorld()); VolumeIterator; ++VolumeIterator)
	{
		VolumeIterator->SpawnTick();
	}
}

//무한 모드 제한 시간도 웨이브도 없음
//몬스터는 계속 나오고 시간이 갈수록 세지며 보스가 주기적으로 끼어듦
//스펙이 세지는 계산은 UMonsterProgressionLibrary가 월드 시간을 보고 하므로 여기서는 낼 때만 정함
void AMainGameModeBase::StartEndlessMode()
{
	//앞으로 스폰될 몬스터를 세기 위해 레벨 맵과 똑같이 스폰 알림을 받음
	//살아있는 수를 세는 이유 클리어 조건은 없지만 HUD가 남은 적 수를 보여줄 수 있어야 함
	GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &AMainGameModeBase::HandleActorSpawned));

	for (TActorIterator<AMonsterBase> MonsterIterator(GetWorld()); MonsterIterator; ++MonsterIterator)
	{
		RegisterMonster(*MonsterIterator);
	}

	GetWorldTimerManager().SetTimer(ContinuousSpawnTimerHandle, this, &AMainGameModeBase::RequestContinuousSpawn, FMath::Max(ContinuousSpawnInterval, 0.1f), true);

	//첫 보스도 주기만큼 기다렸다 나옴 들어가자마자 보스와 마주치지 않게
	GetWorldTimerManager().SetTimer(EndlessBossTimerHandle, this, &AMainGameModeBase::SpawnEndlessBoss, FMath::Max(EndlessBossInterval, 1.0f), true);

	//잠식도는 레벨과 같은 규칙으로 돎 Endless라는 이유로 다르게 굴지 않음
	StartCorruption();
}

void AMainGameModeBase::SpawnEndlessBoss()
{
	//앞 보스가 아직 살아 있어도 또 냄 오래 버틸수록 보스가 쌓이는 것이 무한 모드의 압박
	//스펙은 몬스터가 스스로 월드 시간을 보고 걸기 때문에 나중에 나온 보스일수록 셈
	SpawnBoss();
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

	//보스가 나왔으니 보스 곡으로 갈아 끼움
	//여기서 직접 PlayBGM을 부르지 않는 이유 나올 때와 죽을 때가 같은 판정을 쓰게 하려는 것
	if (Boss)
	{
		UpdateBGMForBoss();
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
	//살아 있는 보스가 더 없으면 원래 곡으로 돌아감
	//Endless는 보스가 쌓이므로 한 마리 죽었다고 바로 돌리면 안 됨
	UpdateBGMForBoss();

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

	//보스는 이제 마지막 웨이브에 레벨마다 자동으로 나오므로 여기서 도전할지 묻지 않음
	//보스를 잡는 것이 다음 레벨 해금 조건이라 건너뛸 수 있으면 조건이 성립하지 않음
	//보스가 아직 살아 있으면 AliveMonsterCount가 0이 아니라 위에서 이미 돌아감
	//보스를 잡으면 HandleBossDead가 남은 잡몹과 상관없이 바로 클리어시킴
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
	GetWorldTimerManager().ClearTimer(WaveTimerHandle);
	GetWorldTimerManager().ClearTimer(ContinuousSpawnTimerHandle);

	//레벨이 끝났으면 잠식도도 멈춤 클리어 화면이 떠 있는 동안 계속 차오르면 안 됨
	GetWorldTimerManager().ClearTimer(CorruptionTimerHandle);

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

	//타이머를 먼저 멈추고 확인하는 이유 죽은 뒤에 독 데미지로 마지막 몬스터가 죽어도 클리어되지 않게
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

	//제한 시간 초과도 잠식도가 다 찬 것과 결과는 같음 잠식 타이머는 StopLevel이 이미 정리했음
	HandleRunFailed();
}

// 배경음

//배경음을 갈아 끼움
void AMainGameModeBase::PlayBGM(USoundBase* NewBGM)
{
	//같은 곡이면 그대로 둠 다시 틀면 처음으로 되감겨서 끊긴 것처럼 들림
	if (CurrentBGM == NewBGM)
	{
		return;
	}

	//이전 곡을 먼저 멈춤 안 멈추면 두 곡이 겹쳐서 흐름
	if (BGMAudio)
	{
		BGMAudio->Stop();
		BGMAudio->DestroyComponent();
		BGMAudio = nullptr;
	}

	CurrentBGM = NewBGM;

	if (!NewBGM)
	{
		return;
	}

	//2D로 재생 배경음은 위치가 없어야 카메라가 어디를 보든 같은 크기로 들림
	//컴포넌트를 들고 있는 이유 곡을 갈아 끼울 때 멈춰야 하고 PlaySound2D는 핸들을 주지 않음
	BGMAudio = UGameplayStatics::SpawnSound2D(this, NewBGM, 1.0f, 1.0f, 0.0f, nullptr, true, false);
}

//지금 맵에 맞는 기본 배경음
USoundBase* AMainGameModeBase::GetBaseBGM() const
{
	const UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>();

	if (!DreamVeilGameInstance)
	{
		return LobbyBGM;
	}

	if (DreamVeilGameInstance->IsInEndless())
	{
		return EndlessBGM;
	}

	//레벨 맵이 아니면 로비나 메인 메뉴 둘 다 싸우지 않는 곳이라 같은 곡을 씀
	return DreamVeilGameInstance->IsInLevelMap() ? LevelBGM : LobbyBGM;
}

//살아 있는 보스가 있으면 보스 곡 없으면 기본 곡
void AMainGameModeBase::UpdateBGMForBoss()
{
	//보스 곡을 안 넣어뒀으면 갈아 끼울 것이 없음
	if (!BossBGM)
	{
		return;
	}

	for (TActorIterator<AMonsterBase> MonsterIterator(GetWorld()); MonsterIterator; ++MonsterIterator)
	{
		AMonsterBase* Monster = *MonsterIterator;

		if (!IsValid(Monster) || !IsBossMonster(Monster))
		{
			continue;
		}

		//죽는 중인 보스는 아직 월드에 남아 있으므로 스탯으로 살아 있는지 확인함
		const UCombatStatsComponent* BossStats = Monster->FindComponentByClass<UCombatStatsComponent>();

		if (BossStats && BossStats->IsDead())
		{
			continue;
		}

		PlayBGM(BossBGM);

		return;
	}

	PlayBGM(GetBaseBGM());
}

//잠식도를 올리기 시작함
void AMainGameModeBase::StartCorruption()
{
	Corruption = 0.0f;

	//0 이하가 되면 타이머가 매 프레임 돌아서 최소값으로 막음
	GetWorldTimerManager().SetTimer(CorruptionTimerHandle, this, &AMainGameModeBase::TickCorruption, FMath::Max(CorruptionTickInterval, 0.01f), true);
}

//주기마다 잠식도를 올림
void AMainGameModeBase::TickCorruption()
{
	//채우는 시간이 0이면 나눌 수 없음 잠식도를 끄고 싶을 때 0을 넣게 두려는 것
	if (CorruptionFillTime <= 0.0f)
	{
		return;
	}

	//한 주기에 오르는 양은 "주기 나누기 총 시간" 총 시간을 바꾸면 속도가 그대로 따라옴
	Corruption += FMath::Max(CorruptionTickInterval, 0.01f) / CorruptionFillTime;

	if (Corruption < 1.0f)
	{
		return;
	}

	Corruption = 1.0f;

	HandleCorruptionFull();
}

//잠식도를 내림
void AMainGameModeBase::ReduceCorruption(float Amount)
{
	//잠식도가 돌고 있지 않으면(로비 메인메뉴 끝난 레벨) 내릴 것도 없음
	if (!CorruptionTimerHandle.IsValid() || Amount <= 0.0f)
	{
		return;
	}

	Corruption = FMath::Max(Corruption - Amount, 0.0f);
}

float AMainGameModeBase::GetCorruption() const
{
	return Corruption;
}

float AMainGameModeBase::GetCorruptionPercent() const
{
	return Corruption * 100.0f;
}

//잠식도가 1에 닿음
void AMainGameModeBase::HandleCorruptionFull()
{
	//잠식 타이머를 먼저 멈춤 게임 오버 창이 떠 있는 동안 계속 돌면 안 됨
	GetWorldTimerManager().ClearTimer(CorruptionTimerHandle);

	//레벨이면 StopLevel이 제한 시간과 웨이브 스폰을 같이 정리함
	//Endless는 LevelTimerHandle이 없어서 false가 돌아오므로 남은 타이머를 직접 멈춤
	if (!StopLevel())
	{
		GetWorldTimerManager().ClearTimer(ContinuousSpawnTimerHandle);
		GetWorldTimerManager().ClearTimer(EndlessBossTimerHandle);
	}

	HandleRunFailed();
}

//한 판이 실패로 끝났을 때의 공통 처리
void AMainGameModeBase::HandleRunFailed()
{
	//죽은 뒤에 끝났으면 로비로 보내지 않음 게임 오버 쪽 흐름과 겹치지 않게
	if (IsPlayerDead())
	{
		return;
	}

	UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>();

	if (!DreamVeilGameInstance)
	{
		return;
	}

	bTimedOut = true;
	if (auto* PC = Cast<AMainPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		if (PC->ShowCorruptionGameOver()) return;
	}
	// Keep the old safe exit if a level has no configured Game Over widget/controller.
	UE_LOG(LogTemp, Error, TEXT("Corruption timeout: Game Over UI unavailable; returning to lobby."));
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
	if (bTimedOut) return 1.0f;
	// Lobby/Endless/finished levels must not look fully corrupted just because there is no timer.
	if (!LevelTimerHandle.IsValid()) return 0.0f;
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
