//시간 기반 예약과 종료 조건을 작은 테스트 월드에서 검증함
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "MainGameModeBase.h"
#include "MonsterSpawnVolume.h"
#include "MonsterBase.h"
#include "CombatStatsComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStageTimedSpawnTest, "DreamVeil.Stage.TimedSpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStageTimedSpawnTest::RunTest(const FString& Parameters)
{
	//BeginPlay를 생략한 테스트 월드에서도 동적 사망 이벤트가 실행되게 허용하고 끝나면 원복함
	TGuardValue<bool> ScriptGuard(GAllowActorScriptExecutionInEditor, true);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	AMainGameModeBase* Mode = World->SpawnActor<AMainGameModeBase>();
	if (!TestNotNull(TEXT("Game mode"), Mode)) { World->DestroyWorld(false); return false; }
	//실제 플레이 맵과 저장 데이터를 건드리지 않고 월드 타이머만 진행함
	Mode->StageProperties.InitialMonsterCount = 4;
	Mode->StageProperties.MonsterCountIncrease = 3;
	Mode->StageProperties.BossSpawnTime = 90.0f;
	Mode->StageProperties.BossClass = AMonsterBase::StaticClass();
	Mode->StartStage();
	TestEqual(TEXT("Initial batch queued"), Mode->PendingSpawnCount, 4);
	TestEqual(TEXT("First spawn waits 0.5 seconds"),
		World->GetTimerManager().GetTimerRemaining(Mode->MonsterSpawnTimerHandle), 0.5f);
	//생성할 볼륨이 없으면 실패한 수량을 잃지 않아야 함
	Mode->SpawnNextMonster();
	TestEqual(TEXT("Failed spawn stays queued"), Mode->PendingSpawnCount, 4);
	//시작점만 과거로 옮겨 경과 시간을 재현함. 증가 콜백은 실제 타이머에서 실행함
	auto AdvanceTimers = [&](float Delta)
	{
		Mode->LevelStartTime -= Delta;
		++GFrameCounter; //타이머 매니저는 같은 프레임의 중복 Tick을 막으므로 테스트 프레임도 진행함
		World->GetTimerManager().Tick(Delta);
	};
	AdvanceTimers(0.0f);
	AdvanceTimers(29.9f);
	TestEqual(TEXT("No growth before 30 seconds"), Mode->PendingSpawnCount, 4);
	AdvanceTimers(0.2f);
	TestEqual(TEXT("30 seconds adds initial plus increment"), Mode->PendingSpawnCount, 11);
	AdvanceTimers(30.0f);
	TestEqual(TEXT("60 seconds adds initial plus two increments"), Mode->PendingSpawnCount, 21);
	TestEqual(TEXT("Hitch still schedules a full spawn interval"),
		World->GetTimerManager().GetTimerRemaining(Mode->MonsterSpawnTimerHandle), 0.5f);
	TestTrue(TEXT("Elapsed game time is exposed"), FMath::IsNearlyEqual(Mode->GetLevelElapsedTime(), 60.1f));

	//보스 생성에 필요한 플레이어만 테스트 월드에 둠. 보스가 나와도 일반 예약은 남으면 안 됨
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	APawn* Player = World->SpawnActor<APawn>();
	if (TestNotNull(TEXT("Player controller"), Controller) && TestNotNull(TEXT("Player pawn"), Player))
	{
		//테스트 월드는 실제 맵처럼 액터 초기화를 돌리지 않으므로 컨트롤러 목록에 직접 등록함
		World->AddController(Controller);
		Controller->Possess(Player);
		TestTrue(TEXT("Test player can be found"), UGameplayStatics::GetPlayerPawn(Mode, 0) == Player);
		AdvanceTimers(30.0f);
		TestTrue(TEXT("Boss starts on elapsed deadline"), Mode->StagePhase == EStagePhase::Boss);
		TestTrue(TEXT("Boss spawned"), Mode->StageBoss.IsValid());
		TestEqual(TEXT("Boss cancels pending normal monsters"), Mode->PendingSpawnCount, 0);
		TestFalse(TEXT("Boss cancels growth"), World->GetTimerManager().TimerExists(Mode->SpawnIncreaseTimerHandle));
		TestFalse(TEXT("Boss cancels staggered spawning"), World->GetTimerManager().TimerExists(Mode->MonsterSpawnTimerHandle));
		AMonsterBase* FirstBoss = Mode->StageBoss.Get();
		Mode->StartBossFight();
		TestTrue(TEXT("Boss only appears once"), Mode->StageBoss.Get() == FirstBoss);
		if (FirstBoss)
		{
			TestTrue(TEXT("Boss tag is assigned"), AMainGameModeBase::IsBossMonster(FirstBoss));
			FirstBoss->FindComponentByClass<UCombatStatsComponent>()->OnDead.Broadcast();
			TestTrue(TEXT("Boss death finishes stage"), Mode->StagePhase == EStagePhase::Finished);
		}
	}
	Mode->StopStage();
	Mode->RequestMonsterSpawn();
	TestEqual(TEXT("Finished stage cannot queue monsters"), Mode->PendingSpawnCount, 0);
	//0마리 설정도 보스 시간은 그대로 흐르며 조기 클리어나 재귀 진행이 없어야 함
	Mode->StageProperties.InitialMonsterCount = 0;
	Mode->StageProperties.MonsterCountIncrease = 0;
	Mode->StartStage();
	TestEqual(TEXT("Zero count has no spawn queue"), Mode->PendingSpawnCount, 0);
	TestTrue(TEXT("Zero count still waits for boss"), Mode->StagePhase == EStagePhase::Combat);
	TestTrue(TEXT("Zero count retains boss timer"), World->GetTimerManager().TimerExists(Mode->BossTimerHandle));
	Mode->HandlePlayerDead();
	TestFalse(TEXT("Death cancels boss timer"), World->GetTimerManager().TimerExists(Mode->BossTimerHandle));
	TestFalse(TEXT("Death cancels growth timer"), World->GetTimerManager().TimerExists(Mode->SpawnIncreaseTimerHandle));
	World->DestroyWorld(false);
	return true;
}
#endif

//이전 구조 학습 메모: 아래 주석은 원문 보존용이며 현재 구현 설명이 아님
// 개발용 자동 테스트. 실제 스포너 안 건드리고 게임모드 진행 조건만 확인함
// 에디터 자동 테스트에서 DreamVeil.Stage.WaveFlow로 찾으면 됨
// 시간 만료 / 전멸 / 즉시 전환이 서로 꼬이지 않는지 확인하는 테스트
// 실제 플레이 맵 말고 테스트용 월드 따로 만듦. 끝나면 지울 거임
// 중간에 생성 실패했어도 테스트 월드는 정리해줘야 됨
// 스포너는 연결 안 함. 완료 알림을 직접 넣어서 순서 바뀌는 상황 확인할 예정
// 첫 웨이브는 휴식 없이 바로 시작하고 생성 완료 답은 기다려야 됨
// 예전 완료 함수나 엉뚱한 ID가 와도 대기 중인 요청 지워지면 안 됨
// 생성 완료만으로는 못 넘어감. 마지막 몬스터까지 죽으면 바로 2웨이브
// 시간 만료는 잔여몹 있어도 바로 다음 웨이브. 대기 중인 요청도 유지함
// 0마리 웨이브도 재귀 없이 넘어가는지 확인
// 테스트용 월드 정리. 실제 맵은 건드린 거 없음
