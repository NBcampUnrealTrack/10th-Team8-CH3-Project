#include "MainGameModeBase.h"
#include "CombatStatsComponent.h"
#include "DreamVeilGameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "MonsterBase.h"
#include "TimerManager.h"

namespace
{
	// 보스 구분용 태그. 여기 이름 바꾸면 BP에 넣은 태그도 맞춰야 됨
	const FName BossTag(TEXT("Boss"));
}

// 맵 들어왔을 때 한 번. 설정 확인하고 바로 1웨이브 ㄱㄱ
void AMainGameModeBase::BeginPlay()
{
	Super::BeginPlay();
	// 로비랑 메뉴도 이 게임모드 쓸 수 있으니 전투 맵인지 먼저 봄
	const UDreamVeilGameInstance* GI = GetGameInstance<UDreamVeilGameInstance>();
	if (!GI || !GI->IsInLevelMap()) return;
	// 일반 1회 + 보스 1회는 있어야 됨. 전투 시간이 0이면 타이머 안 돌아서 최소값 잡음
	StageProperties.TotalWaves = FMath::Max(StageProperties.TotalWaves, 2);
	StageProperties.WaveDuration = FMath::Max(StageProperties.WaveDuration, 0.1f);
	StageProperties.RestDuration = FMath::Max(StageProperties.RestDuration, 0.0f);
	// 마지막은 보스라 일반 수량 배열은 총 횟수보다 한 칸 적어야 됨 걍 원하는 일반페이즈+1하셈
	// 보스 클래스 안 넣었거나 배열 안 맞으면 일단 진행 안 함. BP 설정 확인하세요
	if (StageProperties.NormalWaveMonsterCounts.Num() != StageProperties.TotalWaves - 1 || !StageProperties.BossClass)
	{
		SetStagePhase(EStagePhase::Finished);
		return;
	}
	for (int32& Count : StageProperties.NormalWaveMonsterCounts) Count = FMath::Max(Count, 0);
	bStageActive = true;
	// 이미 맵에 놓여 있는 애들도 카운트 세고, 앞으로 생길 애들은 생성 알림으로 받음
	for (TActorIterator<AMonsterBase> It(GetWorld()); It; ++It) RegisterMonster(*It);
	ActorSpawnedHandle = GetWorld()->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &AMainGameModeBase::HandleActorSpawned));
	// 전체 제한 시간은 별도 옵션. 이거 꺼도 웨이브 시간은 정상적으로 돌아감
	if (bUseLevelTimeLimit)
	{
		GetWorldTimerManager().SetTimer(LevelTimerHandle, this, &AMainGameModeBase::FailLevel,
			FMath::Max(LevelTimeLimit, 0.1f), false);
	}
	StartNextWave();
}

// 맵 나가는데 타이머나 생성 알림 남아 있으면 곤란하니 여서 정리함
void AMainGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bStageActive = false;
	StopStageTimers();
	GetWorld()->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	Super::EndPlay(EndPlayReason);
}

// 상태 변경은 여기로 모음. 같은 상태 또 들어왔다고 UI 알림 두 번 보내진 않음
void AMainGameModeBase::SetStagePhase(EStagePhase NewPhase)
{
	if (StagePhase == NewPhase) return;
	StagePhase = NewPhase;
	OnStagePhaseChanged.Broadcast(StagePhase);
}

// 다음 번호로 진행. 마지막 번호면 일반몹 웨이브 대신 보스전으로 감
void AMainGameModeBase::StartNextWave()
{
	if (!bStageActive || CurrentWave >= StageProperties.TotalWaves) return;
	// 쉬는 동안 죽었을 수도 있으니 다음 전투 열기 전에 확인
	if (IsPlayerDead())
	{
		bStageActive = false;
		SetStagePhase(EStagePhase::Finished);
		StopStageTimers();
		return;
	}
	++CurrentWave;
	if (CurrentWave == StageProperties.TotalWaves) StartBossWave();
	else StartNormalWave();
}

// 일반 전투 시작. 반복 타이머 안 쓰고 이번 전투 끝나는 시각만 한 번 예약함
void AMainGameModeBase::StartNormalWave()
{
	SetStagePhase(EStagePhase::NormalWave);
	GetWorldTimerManager().SetTimer(WaveTimerHandle, this, &AMainGameModeBase::OnWaveTimeExpired,
		StageProperties.WaveDuration, false);
	OnWaveChanged.Broadcast(CurrentWave, StageProperties.TotalWaves);
	RequestWaveSpawn();
	// 수량이 0이거나 요청이 바로 끝난 경우도 있으니 한 번 확인해줌
	TryFinishNormalWaveEarly();
}

// 이번 웨이브 수량 전달. 실제 생성이랑 스포너별 분배는 아직 구현 안 한 자리임
void AMainGameModeBase::RequestWaveSpawn()
{
	const int32 Total = StageProperties.NormalWaveMonsterCounts[CurrentWave - 1];
	// 요청 받자마자 완료 답이 올 수도 있으니 ID부터 넣어놔야 됨
	// 0마리면 새 요청 안 만들고 이전 웨이브에 남은 애들만 기다림
	const int32 RequestId = Total > 0 ? ++NextSpawnRequestId : INDEX_NONE;
	if (RequestId != INDEX_NONE) PendingSpawnRequests.Add(RequestId);
	// 휴식 때문에 멈춰뒀던 생성 요청도 다시 이어가라고 알림
	OnWaveSpawningPausedChanged.Broadcast(false);
	if (RequestId != INDEX_NONE)
		OnWaveSpawnRequested.Broadcast(RequestId, Total, WaveSpawnInterval, StageProperties);
}

// 스포너가 생성 처리 다 끝냈을 때 부를 함수. 실제 연결은 스포너 쪽에서 해줘야 됨
// 모르는 ID나 이미 끝난 ID면 무시해서 같은 완료 알림 두 번 와도 괜찮음
void AMainGameModeBase::NotifySpawnRequestFinished(int32 RequestId)
{
	if (PendingSpawnRequests.Remove(RequestId) > 0) TryFinishNormalWaveEarly();
}

// 예전 BP 호출 받아주는 용도. 대기 요청을 강제로 비우는 건 안 함
void AMainGameModeBase::NotifyAllMonstersSpawned()
{
	TryFinishNormalWaveEarly();
}

// 전투 시간 다 됨. 몬스터 남아 있어도 일단 쉬는 시간으로 넘어감
void AMainGameModeBase::OnWaveTimeExpired()
{
	BeginRest();
}

// 몬스터 다 잡았다고 바로 넘기면 아직 생성 중인 애들 빠질 수 있음
// 생성 대기랑 살아 있는 수 둘 다 0이어야 남은 전투 시간 스킵함
void AMainGameModeBase::TryFinishNormalWaveEarly()
{
	if (bStageActive && StagePhase == EStagePhase::NormalWave
		&& PendingSpawnRequests.IsEmpty() && AliveNormalMonsterCount == 0) BeginRest();
}

// 휴식 시작. 시간 만료랑 마지막 처치가 같이 와도 한 번만 들어오게 상태로 막음
void AMainGameModeBase::BeginRest()
{
	if (!bStageActive || StagePhase != EStagePhase::NormalWave) return;
	if (IsPlayerDead())
	{
		bStageActive = false;
		SetStagePhase(EStagePhase::Finished);
		StopStageTimers();
		return;
	}
	SetStagePhase(EStagePhase::Rest);
	// 전멸로 일찍 왔으면 남은 전투 타이머 버림. 이거 안 끄면 휴식 중에 또 호출됨
	GetWorldTimerManager().ClearTimer(WaveTimerHandle);
	OnWaveSpawningPausedChanged.Broadcast(true);
	// 휴식 0초로 설정했어도 다음 틱으로 넘김. 여기서 바로 재귀 호출하지 않으려고 둠
	if (StageProperties.RestDuration <= 0.0f)
	{
		RestTimerHandle = GetWorldTimerManager().SetTimerForNextTick(this, &AMainGameModeBase::OnRestFinished);
	}
	else
	{
		GetWorldTimerManager().SetTimer(RestTimerHandle, this, &AMainGameModeBase::OnRestFinished,
			StageProperties.RestDuration, false);
	}
}

// 쉬는 시간 끝. 이미 다른 상태면 옛 타이머 알림이니 무시함
void AMainGameModeBase::OnRestFinished()
{
	if (bStageActive && StagePhase == EStagePhase::Rest) StartNextWave();
}

// 마지막 웨이브는 보스 자동 등장. 별도의 잡몹 반복 웨이브는 안 걸어둠
void AMainGameModeBase::StartBossWave()
{
	SetStagePhase(EStagePhase::BossWave);
	OnWaveChanged.Broadcast(CurrentWave, StageProperties.TotalWaves);
	OnWaveSpawningPausedChanged.Broadcast(false);
	// 일단 플레이어 앞에 둠. 보스 전용 스폰 지점 생기면 여기를 바꾸세요
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const FTransform Transform = Player ? FTransform(Player->GetActorRotation(),
		Player->GetActorLocation() + Player->GetActorForwardVector() * BossSpawnDistance) : FTransform::Identity;
	// 바로 BeginPlay 돌리지 않고 태그랑 사망 알림부터 붙이려고 Deferred로 생성함
	AMonsterBase* Boss = Player ? GetWorld()->SpawnActorDeferred<AMonsterBase>(StageProperties.BossClass,
		Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn) : nullptr;
	if (!Boss)
	{
		// 생성 실패했다고 클리어시키면 안 됨. 진행 멈추고 설정 확인할 로그만 남김
		UE_LOG(LogTemp, Error, TEXT("Failed to spawn stage boss. Check BossClass and player pawn."));
		bStageActive = false;
		SetStagePhase(EStagePhase::Finished);
		StopStageTimers();
		return;
	}
	StageBoss = Boss;
	Boss->Tags.AddUnique(BossTag);
	// 태그 붙이기 전에 생성 알림이 먼저 와서 일반몹으로 셌을 수도 있음
	// 그 경우 일반 사망 연결이랑 수량 빼고 보스 사망 연결로 바꿔줌
	if (UCombatStatsComponent* Stats = Boss->FindComponentByClass<UCombatStatsComponent>())
	{
		if (Stats->OnDead.IsAlreadyBound(this, &AMainGameModeBase::HandleMonsterDead))
		{
			Stats->OnDead.RemoveDynamic(this, &AMainGameModeBase::HandleMonsterDead);
			AliveNormalMonsterCount = FMath::Max(AliveNormalMonsterCount - 1, 0);
		}
		Stats->OnDead.AddUniqueDynamic(this, &AMainGameModeBase::HandleBossDead);
	}
	// 설정 붙였으니 이제 생성 마무리. AI 컨트롤러 안 생겼으면 기본 걸로 붙임
	UGameplayStatics::FinishSpawningActor(Boss, Transform);
	if (IsValid(Boss) && !Boss->GetController()) Boss->SpawnDefaultController();
}

// 살아 있는 일반 / 정예 몬스터 세는 곳. 보스는 전멸 조건에서 빼둠
void AMainGameModeBase::RegisterMonster(AMonsterBase* Monster)
{
	if (!bStageActive || !IsValid(Monster) || RegisteredMonsters.Contains(Monster)) return;
	// 스탯 없거나 이미 죽어 있으면 셀 필요 없음. 중복 등록도 위에서 막음
	UCombatStatsComponent* Stats = Monster->FindComponentByClass<UCombatStatsComponent>();
	if (!Stats || Stats->IsDead()) return;
	RegisteredMonsters.Add(Monster);
	if (IsBossMonster(Monster)) return;
	++AliveNormalMonsterCount;
	Stats->OnDead.AddUniqueDynamic(this, &AMainGameModeBase::HandleMonsterDead);
}

// 월드 생성 알림은 몬스터 말고도 오니까 캐스팅해서 넘김. 아니면 nullptr라 그냥 무시됨
void AMainGameModeBase::HandleActorSpawned(AActor* SpawnedActor)
{
	RegisterMonster(Cast<AMonsterBase>(SpawnedActor));
}

// 한 마리 죽었으니 빼고 전멸인지 봄. 이전 웨이브 잔여몹도 같은 수량에서 빠짐
void AMainGameModeBase::HandleMonsterDead()
{
	AliveNormalMonsterCount = FMath::Max(AliveNormalMonsterCount - 1, 0);
	TryFinishNormalWaveEarly();
}

// 보스전 중 보스 죽었을 때만 클리어. 다른 상태에서는 건너뜀
void AMainGameModeBase::HandleBossDead()
{
	if (StagePhase == EStagePhase::BossWave) CompleteStage();
}

// 진행 타이머 전부 정리. 스포너에는 취소 알림만 보내니 실제 취소는 거기서 연결해야 됨
void AMainGameModeBase::StopStageTimers()
{
	GetWorldTimerManager().ClearTimer(WaveTimerHandle);
	GetWorldTimerManager().ClearTimer(RestTimerHandle);
	GetWorldTimerManager().ClearTimer(LevelTimerHandle);
	PendingSpawnRequests.Empty();
	OnWaveSpawningCancelled.Broadcast();
}

// 클리어는 한 번만 처리. 먼저 진행 끄고 나서 저장이랑 맵 이동 쪽에 넘김
void AMainGameModeBase::CompleteStage()
{
	if (!bStageActive) return;
	bStageActive = false;
	SetStagePhase(EStagePhase::Finished);
	StopStageTimers();
	if (IsPlayerDead()) return;
	// 지금 GameInstance는 클리어 후 로비로 감. 다음 맵 직행은 그쪽 수정할 때 연결할 예정
	if (UDreamVeilGameInstance* GI = GetGameInstance<UDreamVeilGameInstance>()) GI->CompleteCurrentLevel();
}

// 전체 제한 시간 켜뒀을 때 시간 초과 처리. 죽은 상태면 게임 오버 흐름에 맡김
void AMainGameModeBase::FailLevel()
{
	if (!bStageActive) return;
	bStageActive = false;
	SetStagePhase(EStagePhase::Finished);
	StopStageTimers();
	if (IsPlayerDead()) return;
	if (UDreamVeilGameInstance* GI = GetGameInstance<UDreamVeilGameInstance>()) GI->FailCurrentLevel();
}

// 플레이어 체력 컴포넌트에 물어봄. 폰이나 컴포넌트 없다고 죽었다고 보지는 않음
bool AMainGameModeBase::IsPlayerDead() const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const UCombatStatsComponent* Stats = Player ? Player->FindComponentByClass<UCombatStatsComponent>() : nullptr;
	return Stats && Stats->IsDead();
}

// 보스 클래스 따로 비교하지 않고 태그로 판정. 다른 코드에서도 이거 쓰세요
bool AMainGameModeBase::IsBossMonster(const AActor* Actor)
{
	return Actor && Actor->ActorHasTag(BossTag);
}

// UI에서 현재 구간 카운트다운 띄울 때 쓰는 거. 보스전은 별도 시간 없어서 0
float AMainGameModeBase::GetPhaseTimeRemaining() const
{
	if (!bStageActive) return 0.0f;
	if (StagePhase == EStagePhase::NormalWave) return FMath::Max(0.0f, GetWorldTimerManager().GetTimerRemaining(WaveTimerHandle));
	if (StagePhase == EStagePhase::Rest) return FMath::Max(0.0f, GetWorldTimerManager().GetTimerRemaining(RestTimerHandle));
	return 0.0f;
}

// 전체 제한 시간 남은 초. 타이머 없을 때 엔진이 음수 줄 수 있어서 0으로 막음
float AMainGameModeBase::GetLevelTimeRemaining() const
{
	return FMath::Max(0.0f, GetWorldTimerManager().GetTimerRemaining(LevelTimerHandle));
}

// 전체 제한 안 쓰면 0 반환. 켜뒀을 때는 실제 타이머에 넣은 최소값하고 맞춤
float AMainGameModeBase::GetLevelTimeLimit() const
{
	return bUseLevelTimeLimit ? FMath::Max(LevelTimeLimit, 0.1f) : 0.0f;
}

// 전체 시간 기준 잠식 비율. 제한 시간 꺼뒀으면 0이고 켜뒀으면 지날수록 1에 가까워짐
float AMainGameModeBase::GetLevelTimeProgress() const
{
	const float Limit = GetLevelTimeLimit();
	return Limit > 0.0f ? FMath::Clamp(1.0f - GetLevelTimeRemaining() / Limit, 0.0f, 1.0f) : 0.0f;
}
