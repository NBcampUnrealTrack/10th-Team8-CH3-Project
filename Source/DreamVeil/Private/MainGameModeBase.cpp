#include "MainGameModeBase.h"
#include "CombatStatsComponent.h"
#include "DreamVeilGameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "MonsterBase.h"
#include "MonsterSpawnVolume.h"
#include "TimerManager.h"

namespace
{
	const FName BossTag(TEXT("Boss"));
}

void AMainGameModeBase::BeginPlay()
{
	Super::BeginPlay();
	//로비와 메뉴에서는 전투를 시작하지 않음. 맵 구분 책임은 GameInstance에 유지함
	const UDreamVeilGameInstance* GI = GetGameInstance<UDreamVeilGameInstance>();
	if (!GI || !GI->IsInLevelMap()) return;
	for (TActorIterator<AMonsterSpawnVolume> It(GetWorld()); It; ++It) SpawnVolumes.Add(*It);
	if (!StageProperties.BossClass || SpawnVolumes.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Stage requires BossClass and at least one MonsterSpawnVolume."));
		SetStagePhase(EStagePhase::Finished);
		return;
	}
	if (APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (UCombatStatsComponent* Stats = Player->FindComponentByClass<UCombatStatsComponent>())
			Stats->OnDead.AddUniqueDynamic(this, &AMainGameModeBase::HandlePlayerDead);
	}
	StartStage();
}

void AMainGameModeBase::StartStage()
{
	//편집 UI의 최소값만 믿지 않고 실제 타이머에도 최소 간격을 보장함
	StageProperties.SpawnIncreaseInterval = FMath::Max(StageProperties.SpawnIncreaseInterval, 0.1f);
	StageProperties.MonsterSpawnInterval = FMath::Max(StageProperties.MonsterSpawnInterval, 0.05f);
	StageProperties.BossSpawnTime = FMath::Max(StageProperties.BossSpawnTime, 0.1f);
	LevelStartTime = GetWorld()->GetTimeSeconds();
	bStageActive = true;
	SetStagePhase(EStagePhase::Combat);
	//보스 시간은 소환 완료나 몬스터 처치와 무관하게 따로 예약함
	GetWorld()->GetTimerManager().SetTimer(BossTimerHandle, this, &AMainGameModeBase::StartBossFight,
		StageProperties.BossSpawnTime, false);
	//프레임이 오래 멈췄다가 돌아와도 증가 요청을 같은 프레임에 여러 번 중복 처리하지 않음
	FTimerManagerTimerParameters IncreaseParameters;
	IncreaseParameters.bLoop = true;
	IncreaseParameters.bMaxOncePerFrame = true;
	GetWorld()->GetTimerManager().SetTimer(SpawnIncreaseTimerHandle, this, &AMainGameModeBase::RequestMonsterSpawn,
		StageProperties.SpawnIncreaseInterval, IncreaseParameters);
	if (bUseLevelTimeLimit)
		GetWorld()->GetTimerManager().SetTimer(LevelTimerHandle, this, &AMainGameModeBase::FailLevel,
			FMath::Max(LevelTimeLimit, 0.1f), false);
	//첫 묶음은 즉시 예약하지만 첫 마리도 설정한 간격이 지난 뒤 등장함
	RequestMonsterSpawn();
}

void AMainGameModeBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bStageActive = false;
	StopStageTimers();
	Super::EndPlay(EndPlayReason);
}

void AMainGameModeBase::SetStagePhase(EStagePhase NewPhase)
{
	if (StagePhase == NewPhase) return;
	StagePhase = NewPhase;
	OnStagePhaseChanged.Broadcast(StagePhase);
}

float AMainGameModeBase::GetLevelElapsedTime() const
{
	return GetWorld() ? FMath::Max(0.0f, static_cast<float>(GetWorld()->GetTimeSeconds() - LevelStartTime)) : 0.0f;
}

void AMainGameModeBase::RequestMonsterSpawn()
{
	if (!bStageActive || StagePhase != EStagePhase::Combat) return;
	if (IsPlayerDead()) { StopStage(); return; }
	//두 예약이 같은 프레임에 만료되어도 보스 시각 이후 일반 몬스터를 추가하지 않음
	if (GetLevelElapsedTime() >= StageProperties.BossSpawnTime) { StartBossFight(); return; }
	//수량을 별도 상태로 중복 저장하지 않고 경과 시간으로 계산함: 기본 + 지난 주기 수 * 증가량
	const int64 Count = FMath::Max(StageProperties.InitialMonsterCount, 0)
		+ static_cast<int64>(FMath::FloorToInt(GetLevelElapsedTime() / StageProperties.SpawnIncreaseInterval))
		* FMath::Max(StageProperties.MonsterCountIncrease, 0);
	//기존 예약은 유지하고 극단적인 설정에서도 정수 오버플로를 막음
	PendingSpawnCount = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(PendingSpawnCount) + Count, MAX_int32));
	if (PendingSpawnCount > 0 && !GetWorld()->GetTimerManager().IsTimerActive(MonsterSpawnTimerHandle))
		GetWorld()->GetTimerManager().SetTimer(MonsterSpawnTimerHandle, this, &AMainGameModeBase::SpawnNextMonster,
			StageProperties.MonsterSpawnInterval, false);
}

void AMainGameModeBase::SpawnNextMonster()
{
	if (!bStageActive || StagePhase != EStagePhase::Combat) return;
	if (IsPlayerDead()) { StopStage(); return; }
	if (GetLevelElapsedTime() >= StageProperties.BossSpawnTime) { StartBossFight(); return; }
	SpawnVolumes.RemoveAll([](const TWeakObjectPtr<AMonsterSpawnVolume>& Volume) { return !Volume.IsValid(); });
	if (PendingSpawnCount > 0 && !SpawnVolumes.IsEmpty())
	{
		//여러 볼륨이 각각 타이머를 돌리면 동시에 나오므로 전역 예약에서 딱 한 볼륨만 선택함
		if (SpawnVolumes[FMath::RandRange(0, SpawnVolumes.Num() - 1)]->TrySpawnMonster()) --PendingSpawnCount;
		//NavMesh나 충돌 때문에 실패하면 수량을 차감하지 않고 다음 간격에 다시 시도함
	}
	//반복 타이머의 밀린 호출이 한 프레임에 몰리지 않게 매번 한 번짜리 예약을 새로 걸음
	if (bStageActive && StagePhase == EStagePhase::Combat && PendingSpawnCount > 0)
		GetWorld()->GetTimerManager().SetTimer(MonsterSpawnTimerHandle, this, &AMainGameModeBase::SpawnNextMonster,
			StageProperties.MonsterSpawnInterval, false);
}

void AMainGameModeBase::StartBossFight()
{
	if (!bStageActive || StagePhase != EStagePhase::Combat) return;
	if (IsPlayerDead()) { StopStage(); return; }
	GetWorld()->GetTimerManager().ClearTimer(BossTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(SpawnIncreaseTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(MonsterSpawnTimerHandle);
	PendingSpawnCount = 0;
	SetStagePhase(EStagePhase::Boss);
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const FTransform Transform = Player ? FTransform(Player->GetActorRotation(),
		Player->GetActorLocation() + Player->GetActorForwardVector() * BossSpawnDistance) : FTransform::Identity;
	//BeginPlay보다 먼저 Boss 태그와 사망 이벤트를 붙이기 위해 Deferred로 생성함
	AMonsterBase* Boss = Player ? GetWorld()->SpawnActorDeferred<AMonsterBase>(StageProperties.BossClass,
		Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding) : nullptr;
	if (!Boss)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to spawn boss. Check BossClass, player pawn and spawn space."));
		StopStage();
		return;
	}
	StageBoss = Boss;
	Boss->Tags.AddUnique(BossTag);
	if (UCombatStatsComponent* Stats = Boss->FindComponentByClass<UCombatStatsComponent>())
		Stats->OnDead.AddUniqueDynamic(this, &AMainGameModeBase::HandleBossDead);
	UGameplayStatics::FinishSpawningActor(Boss, Transform);
	//Deferred 생성은 마무리 단계에서도 충돌로 취소될 수 있으므로 보스 없는 보스전에 남지 않게 확인함
	if (!IsValid(Boss))
	{
		UE_LOG(LogTemp, Error, TEXT("Boss spawn was cancelled during construction. Check spawn space."));
		StopStage();
		return;
	}
	if (!Boss->GetController()) Boss->SpawnDefaultController();
}

void AMainGameModeBase::HandleBossDead()
{
	if (StagePhase == EStagePhase::Boss) CompleteStage();
}

void AMainGameModeBase::HandlePlayerDead()
{
	StopStage();
}

void AMainGameModeBase::StopStage()
{
	if (!bStageActive) return;
	bStageActive = false;
	StopStageTimers();
	SetStagePhase(EStagePhase::Finished);
}

void AMainGameModeBase::StopStageTimers()
{
	GetWorld()->GetTimerManager().ClearTimer(SpawnIncreaseTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(MonsterSpawnTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(BossTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(LevelTimerHandle);
	PendingSpawnCount = 0;
}

void AMainGameModeBase::CompleteStage()
{
	if (!bStageActive) return;
	StopStage();
	if (IsPlayerDead()) return;
	if (UDreamVeilGameInstance* GI = GetGameInstance<UDreamVeilGameInstance>()) GI->CompleteCurrentLevel();
}

void AMainGameModeBase::FailLevel()
{
	if (!bStageActive) return;
	StopStage();
	if (IsPlayerDead()) return;
	if (UDreamVeilGameInstance* GI = GetGameInstance<UDreamVeilGameInstance>()) GI->FailCurrentLevel();
}

bool AMainGameModeBase::IsPlayerDead() const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const UCombatStatsComponent* Stats = Player ? Player->FindComponentByClass<UCombatStatsComponent>() : nullptr;
	return Stats && Stats->IsDead();
}

bool AMainGameModeBase::IsBossMonster(const AActor* Actor)
{
	return Actor && Actor->ActorHasTag(BossTag);
}

float AMainGameModeBase::GetPhaseTimeRemaining() const
{
	return bStageActive && StagePhase == EStagePhase::Combat
		? FMath::Max(0.0f, GetWorldTimerManager().GetTimerRemaining(BossTimerHandle)) : 0.0f;
}

float AMainGameModeBase::GetLevelTimeRemaining() const
{
	return FMath::Max(0.0f, GetWorldTimerManager().GetTimerRemaining(LevelTimerHandle));
}

float AMainGameModeBase::GetLevelTimeLimit() const
{
	return bUseLevelTimeLimit ? FMath::Max(LevelTimeLimit, 0.1f) : 0.0f;
}

float AMainGameModeBase::GetLevelTimeProgress() const
{
	const float Limit = GetLevelTimeLimit();
	return Limit > 0.0f ? FMath::Clamp(1.0f - GetLevelTimeRemaining() / Limit, 0.0f, 1.0f) : 0.0f;
}

//이전 구조 학습 메모: 아래 주석은 원문 보존용이며 현재 구현 설명이 아님
// 보스 구분용 태그. 여기 이름 바꾸면 BP에 넣은 태그도 맞춰야 됨
// 맵 들어왔을 때 한 번. 설정 확인하고 바로 1웨이브
// 로비랑 메뉴도 이 게임모드 쓸 수 있으니 전투 맵인지 먼저 봄
// 일반 1회 + 보스 1회는 있어야 됨. 전투 시간이 0이면 타이머 안 돌아서 최소값 잡음
// 마지막은 보스라 일반 수량 배열은 총 횟수보다 한 칸 적어야 됨 걍 원하는 일반페이즈+1하셈
// 보스 클래스 안 넣었거나 배열 안 맞으면 일단 진행 안 함. BP 설정 확인하세요
// 이미 맵에 놓여 있는 애들도 카운트 세고, 앞으로 생길 애들은 생성 알림으로 받음
// 전체 제한 시간은 별도 옵션. 이거 꺼도 웨이브 시간은 정상적으로 돌아감
// 맵 나가는데 타이머나 생성 알림 남아 있으면 곤란하니 여서 정리함
// 상태 변경은 여기로 모음. 같은 상태 또 들어왔다고 UI 알림 두 번 보내진 않음
// 다음 번호로 진행. 마지막 번호면 일반몹 웨이브 대신 보스전으로 감
// 0마리 웨이브나 즉시 완료 답도 재귀 없이 바로 넘김
// 일반 전투 시작. 반복 타이머 안 쓰고 이번 전투 끝나는 시각만 한 번 예약함
// 수량이 0이거나 요청이 바로 끝난 경우도 있으니 한 번 확인해줌
// 이번 웨이브 수량 전달. 실제 생성이랑 스포너별 분배는 아직 구현 안 한 자리임
// 요청 받자마자 완료 답이 올 수도 있으니 ID부터 넣어놔야 됨
// 0마리면 새 요청 안 만들고 이전 웨이브에 남은 애들만 기다림
// 스포너가 생성 처리 다 끝냈을 때 부를 함수. 실제 연결은 스포너 쪽에서 해줘야 됨
// 모르는 ID나 이미 끝난 ID면 무시해서 같은 완료 알림 두 번 와도 괜찮음
// 예전 BP 호출 받아주는 용도. 대기 요청을 강제로 비우는 건 안 함
// 옛 웨이브의 시간 만료 알림이 새 웨이브까지 넘기지 않게 번호 확인함
// 생성 대기랑 살아 있는 몬스터 둘 다 없으면 남은 시간 버리고 바로 다음 웨이브
// 마지막 웨이브는 보스 자동 등장. 별도의 잡몹 반복 웨이브는 안 걸어둠
// 일단 플레이어 앞에 둠. 보스 전용 스폰 지점 생기면 여기를 바꾸세요
// 바로 BeginPlay 돌리지 않고 태그랑 사망 알림부터 붙이려고 Deferred로 생성함
// 생성 실패했다고 클리어시키면 안 됨. 진행 멈추고 설정 확인할 로그만 남김
// 태그 붙이기 전에 생성 알림이 먼저 와서 일반몹으로 셌을 수도 있음
// 그 경우 일반 사망 연결이랑 수량 빼고 보스 사망 연결로 바꿔줌
// 설정 붙였으니 이제 생성 마무리. AI 컨트롤러 안 생겼으면 기본 걸로 붙임
// 살아 있는 일반 / 정예 몬스터 세는 곳. 보스는 전멸 조건에서 빼둠
// 스탯 없거나 이미 죽어 있으면 셀 필요 없음. 중복 등록도 위에서 막음
// 월드 생성 알림은 몬스터 말고도 오니까 캐스팅해서 넘김. 아니면 nullptr라 그냥 무시됨
// 한 마리 죽었으니 빼고 전멸인지 봄. 이전 웨이브 잔여몹도 같은 수량에서 빠짐
// 보스전 중 보스 죽었을 때만 클리어. 다른 상태에서는 건너뜀
// 진행 타이머 전부 정리. 스포너에는 취소 알림만 보내니 실제 취소는 거기서 연결해야 됨
// 클리어는 한 번만 처리. 먼저 진행 끄고 나서 저장이랑 맵 이동 쪽에 넘김
// 지금 GameInstance는 클리어 후 로비로 감. 다음 맵 직행은 그쪽 수정할 때 연결할 예정
// 전체 제한 시간 켜뒀을 때 시간 초과 처리. 죽은 상태면 게임 오버 흐름에 맡김
// 플레이어 체력 컴포넌트에 물어봄. 폰이나 컴포넌트 없다고 죽었다고 보지는 않음
// 보스 클래스 따로 비교하지 않고 태그로 판정. 다른 코드에서도 이거 쓰세요
// UI에서 현재 구간 카운트다운 띄울 때 쓰는 거. 보스전은 별도 시간 없어서 0
// 전체 제한 시간 남은 초. 타이머 없을 때 엔진이 음수 줄 수 있어서 0으로 막음
// 전체 제한 안 쓰면 0 반환. 켜뒀을 때는 실제 타이머에 넣은 최소값하고 맞춤
// 전체 시간 기준 잠식 비율. 제한 시간 꺼뒀으면 0이고 켜뒀으면 지날수록 1에 가까워짐
