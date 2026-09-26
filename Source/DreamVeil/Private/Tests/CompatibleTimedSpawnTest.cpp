//기존 UI 계약을 유지하면서 시간 기반 소환으로 바뀌었는지 검증함. 실제 맵/세이브는 사용하지 않음
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "MainGameModeBase.h"
#include "MonsterSpawnVolume.h"
#include "MonsterBase.h"
#include "CombatStatsComponent.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompatibleTimedSpawnTest, "DreamVeil.Stage.CompatibleTimedSpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCompatibleTimedSpawnTest::RunTest(const FString& Parameters)
{
	//초기화를 생략한 테스트 월드에서도 동적 사망 이벤트가 실행되도록 허용하고 종료 시 원복함
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	AMainGameModeBase* Mode = World->SpawnActor<AMainGameModeBase>();
	AMonsterSpawnVolume* Volume = World->SpawnActor<AMonsterSpawnVolume>();
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	APawn* Player = World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("Game mode"), Mode) || !TestNotNull(TEXT("Spawn volume"), Volume)
		|| !TestNotNull(TEXT("Controller"), Controller) || !TestNotNull(TEXT("Player"), Player))
	{
		World->DestroyWorld(false);
		return false;
	}
	World->AddController(Controller);
	Controller->Possess(Player);
	TestTrue(TEXT("Player can be found"), UGameplayStatics::GetPlayerPawn(Mode, 0) == Player);

	//표시 상한을 2로 내려도 60초 이후 생성이 중단되지 않는지 확인함
	Mode->WaveCount = 2;
	Mode->MonsterCountIncrease = 3;
	Mode->BossSpawnTime = 90.0f;
	Mode->BossClass = AMonsterBase::StaticClass();
	World->GetTimerManager().SetTimer(Mode->LevelTimerHandle, Mode, &AMainGameModeBase::FailLevel, Mode->LevelTimeLimit, false);
	World->GetTimerManager().SetTimer(Mode->BossTimerHandle, Mode, &AMainGameModeBase::HandleBossTimeReached, Mode->BossSpawnTime, false);
	Mode->StartNextWave();
	TestEqual(TEXT("Initial total count is queued"), Mode->PendingSpawnCount, 4);
	TestEqual(TEXT("Existing HUD starts at one"), Mode->GetCurrentWave(), 1);
	TestEqual(TEXT("First monster waits 0.5 seconds"), World->GetTimerManager().GetTimerRemaining(Mode->MonsterSpawnTimerHandle), 0.5f);
	Mode->NotifyAllMonstersSpawned();
	TestFalse(TEXT("Legacy completion cannot end time progression early"), Mode->bAllMonstersSpawned);
	Mode->AcceptBossChallenge();
	Mode->DeclineBossChallenge();
	TestFalse(TEXT("Legacy buttons cannot summon an early boss"), Mode->bBossSpawned);
	TestTrue(TEXT("Legacy buttons cannot complete the level early"), Mode->LevelTimerHandle.IsValid());

	//실제로 기다리지 않고 월드 타이머를 진행함. 같은 프레임의 중복 Tick 방지를 피하려고 프레임도 증가시킴
	auto Advance = [&](float Delta)
	{
		++GFrameCounter;
		World->GetTimerManager().Tick(Delta);
	};
	Advance(0.0f);
	Advance(29.9f);
	TestEqual(TEXT("No growth before deadline; failed NavMesh spawn stays queued"), Mode->PendingSpawnCount, 4);
	Advance(0.2f);
	TestEqual(TEXT("30 seconds adds seven"), Mode->PendingSpawnCount, 11);
	Advance(30.0f);
	TestEqual(TEXT("60 seconds adds ten despite display cap"), Mode->PendingSpawnCount, 21);
	TestEqual(TEXT("HUD stays within existing WaveCount"), Mode->GetCurrentWave(), 2);
	TestEqual(TEXT("Hitch still leaves a full spawn interval"), World->GetTimerManager().GetTimerRemaining(Mode->MonsterSpawnTimerHandle), 0.5f);
	TestTrue(TEXT("Remaining time keeps original UI semantics"), FMath::IsNearlyEqual(Mode->GetLevelTimeRemaining(), 119.9f, 0.01f));
	TestTrue(TEXT("Corruption remains elapsed / LevelTimeLimit"), FMath::IsNearlyEqual(Mode->GetLevelTimeProgress(), 60.1f / 180.0f, 0.001f));

	//기존 BP가 SpawnWave로 직접 예약한 수량도 보스 시작 시 함께 취소되어야 함
	Volume->SpawnWave(3, 0.5f);
	TestTrue(TEXT("Legacy SpawnWave still queues"), Volume->HasPendingSpawns());
	Advance(30.0f);
	TestTrue(TEXT("Boss starts at configured time"), Mode->bBossSpawned);
	TestEqual(TEXT("Boss cancels global queue"), Mode->PendingSpawnCount, 0);
	TestFalse(TEXT("Boss cancels legacy volume queue"), Volume->HasPendingSpawns());
	TestFalse(TEXT("Boss stops growth timer"), World->GetTimerManager().TimerExists(Mode->WaveTimerHandle));
	TestFalse(TEXT("Boss stops stagger timer"), World->GetTimerManager().TimerExists(Mode->MonsterSpawnTimerHandle));
	TestTrue(TEXT("Boss fight keeps UI countdown running"), Mode->LevelTimerHandle.IsValid());
	Mode->AcceptBossChallenge();
	Mode->HandleBossTimeReached();
	AMonsterBase* Boss = nullptr;
	int32 BossCount = 0;
	for (TActorIterator<AMonsterBase> It(World); It; ++It)
	{
		if (AMainGameModeBase::IsBossMonster(*It)) { Boss = *It; ++BossCount; }
	}
	TestEqual(TEXT("Repeated callbacks cannot create another boss"), BossCount, 1);
	if (Boss) Boss->FindComponentByClass<UCombatStatsComponent>()->OnDead.Broadcast();
	TestFalse(TEXT("Boss death ends level exactly once"), Mode->LevelTimerHandle.IsValid());
	TestEqual(TEXT("Finished level keeps original zero remaining time"), Mode->GetLevelTimeRemaining(), 0.0f);

	//보스 클래스가 없는 기존 맵은 종료 시각 이전에 조기 클리어하지 않고 종료 시각 이후 전멸로 완료함
	Mode->BossClass = nullptr;
	Mode->bBossSpawned = false;
	Mode->bAllMonstersSpawned = false;
	Mode->MonstersPerWave = 0;
	Mode->MonsterCountIncrease = 0;
	Mode->BossSpawnTime = 30.0f;
	World->GetTimerManager().SetTimer(Mode->LevelTimerHandle, Mode, &AMainGameModeBase::FailLevel, 180.0f, false);
	World->GetTimerManager().SetTimer(Mode->BossTimerHandle, Mode, &AMainGameModeBase::HandleBossTimeReached, 30.0f, false);
	Mode->StartNextWave();
	Mode->NotifyAllMonstersSpawned();
	TestTrue(TEXT("Empty level still waits for configured time"), Mode->LevelTimerHandle.IsValid());
	Advance(0.0f);
	Advance(30.1f);
	TestFalse(TEXT("Boss-free level completes after deadline and all spawns"), Mode->LevelTimerHandle.IsValid());

	//죽음으로 끝날 때도 전체 제한/증가/보스/볼륨 예약이 모두 정리되는지 확인함
	Mode->bAllMonstersSpawned = false;
	Mode->MonstersPerWave = 4;
	World->GetTimerManager().SetTimer(Mode->LevelTimerHandle, Mode, &AMainGameModeBase::FailLevel, 180.0f, false);
	World->GetTimerManager().SetTimer(Mode->BossTimerHandle, Mode, &AMainGameModeBase::HandleBossTimeReached, 30.0f, false);
	Mode->StartNextWave();
	Volume->SpawnWave(2, 0.5f);
	Mode->HandlePlayerDead();
	TestFalse(TEXT("Death stops level timer"), Mode->LevelTimerHandle.IsValid());
	TestFalse(TEXT("Death stops boss timer"), World->GetTimerManager().TimerExists(Mode->BossTimerHandle));
	TestFalse(TEXT("Death stops volume queue"), Volume->HasPendingSpawns());
	Mode->RequestWaveSpawn();
	TestEqual(TEXT("Late callback cannot restart spawning"), Mode->PendingSpawnCount, 0);
	World->DestroyWorld(false);
	return true;
}
#endif
