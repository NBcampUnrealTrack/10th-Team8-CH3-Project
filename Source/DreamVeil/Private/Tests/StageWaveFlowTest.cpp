// 개발용 자동 테스트. 실제 스포너 안 건드리고 게임모드 진행 조건만 확인함
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "MainGameModeBase.h"
#include "TimerManager.h"

// 에디터 자동 테스트에서 DreamVeil.Stage.WaveFlow로 찾으면 됨
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStageWaveFlowTest, "DreamVeil.Stage.WaveFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 시간 만료 / 전멸 / 휴식 전환이 서로 꼬이지 않는지 확인하는 테스트
bool FStageWaveFlowTest::RunTest(const FString& Parameters)
{
	// 실제 플레이 맵 말고 테스트용 월드 따로 만듦. 끝나면 지울 거임
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World)) return false;
	AMainGameModeBase* Mode = World->SpawnActor<AMainGameModeBase>();
	if (!TestNotNull(TEXT("Game mode"), Mode))
	{
		// 중간에 생성 실패했어도 테스트 월드는 정리해줘야 됨
		World->DestroyWorld(false);
		return false;
	}
	// 스포너는 연결 안 함. 완료 알림을 직접 넣어서 순서 바뀌는 상황 확인할 예정
	Mode->StageProperties.NormalWaveMonsterCounts = { 5, 7, 9, 11, 13 };
	Mode->bStageActive = true;
	Mode->StartNextWave();
	// 첫 웨이브는 휴식 없이 바로 시작하고 생성 완료 답은 기다려야 됨
	TestEqual(TEXT("First wave starts immediately"), Mode->CurrentWave, 1);
	TestTrue(TEXT("First wave is combat"), Mode->StagePhase == EStagePhase::NormalWave);
	TestEqual(TEXT("Wave request tracked until completion"), Mode->PendingSpawnRequests.Num(), 1);
	// 예전 완료 함수나 엉뚱한 ID가 와도 대기 중인 요청 지워지면 안 됨
	Mode->NotifyAllMonstersSpawned();
	TestTrue(TEXT("Legacy notification cannot bypass pending request"), Mode->StagePhase == EStagePhase::NormalWave);
	Mode->NotifySpawnRequestFinished(999);
	TestEqual(TEXT("Unknown completion ignored"), Mode->PendingSpawnRequests.Num(), 1);

	// 생성은 끝났는데 한 마리 살아 있는 상황. 아직 휴식 들어가면 안 됨
	Mode->AliveNormalMonsterCount = 1;
	Mode->NotifySpawnRequestFinished(1);
	TestTrue(TEXT("Living monster prevents early rest"), Mode->StagePhase == EStagePhase::NormalWave);
	// 마지막 한 마리 잡으면 남은 전투 시간 버리고 10초 휴식으로 가는지 봄
	Mode->HandleMonsterDead();
	TestTrue(TEXT("Final death starts rest"), Mode->StagePhase == EStagePhase::Rest);
	TestFalse(TEXT("Early clear cancels combat deadline"), World->GetTimerManager().TimerExists(Mode->WaveTimerHandle));
	TestEqual(TEXT("Rest lasts ten seconds"), Mode->GetPhaseTimeRemaining(), 10.0f);
	// 끝난 요청 답이 또 오거나 옛 전투 타이머가 불려도 휴식 건너뛰면 안 됨
	Mode->NotifySpawnRequestFinished(1);
	Mode->OnWaveTimeExpired();
	TestEqual(TEXT("Duplicate completion and stale deadline do not advance rest"), Mode->CurrentWave, 1);
	// 실제로 10초 기다리는 대신 타이머 만료 상황을 직접 만들어줌
	World->GetTimerManager().ClearTimer(Mode->RestTimerHandle);
	Mode->OnRestFinished();
	Mode->OnRestFinished();
	TestEqual(TEXT("Rest completion advances once"), Mode->CurrentWave, 2);

	// 이번엔 몬스터 남은 채 시간 끝나는 경우. 잔여몹이랑 생성 대기 둘 다 유지돼야 됨
	Mode->AliveNormalMonsterCount = 2;
	Mode->OnWaveTimeExpired();
	TestTrue(TEXT("Deadline starts rest despite living monsters"), Mode->StagePhase == EStagePhase::Rest);
	TestEqual(TEXT("Living monsters carry over"), Mode->AliveNormalMonsterCount, 2);
	TestEqual(TEXT("Pending spawn request survives rest"), Mode->PendingSpawnRequests.Num(), 1);
	// 다음 웨이브 열어도 이전 요청은 완료 답 오기 전까진 남아 있어야 됨
	World->GetTimerManager().ClearTimer(Mode->RestTimerHandle);
	Mode->OnRestFinished();
	TestEqual(TEXT("Next wave starts with leftovers"), Mode->CurrentWave, 3);
	TestEqual(TEXT("Previous and current requests are tracked"), Mode->PendingSpawnRequests.Num(), 2);
	// 이전 생성 완료 + 남은 몹 전멸이어도 이번 생성이 안 끝났으면 아직 전투임
	Mode->NotifySpawnRequestFinished(2);
	Mode->HandleMonsterDead();
	Mode->HandleMonsterDead();
	TestTrue(TEXT("Current request still blocks early rest"), Mode->StagePhase == EStagePhase::NormalWave);
	// 마지막 처치보다 생성 완료가 늦게 와도 그때 휴식 들어가는지 확인
	Mode->NotifySpawnRequestFinished(3);
	TestTrue(TEXT("Completion after last death also starts rest"), Mode->StagePhase == EStagePhase::Rest);

	// 판 끝났으면 요청이랑 타이머 다 지우고 늦게 온 콜백도 무시해야 됨
	Mode->bStageActive = false;
	Mode->SetStagePhase(EStagePhase::Finished);
	Mode->StopStageTimers();
	TestTrue(TEXT("Stage stop clears requests"), Mode->PendingSpawnRequests.IsEmpty());
	TestFalse(TEXT("Stage stop clears rest timer"), World->GetTimerManager().TimerExists(Mode->RestTimerHandle));
	Mode->OnRestFinished();
	TestEqual(TEXT("Finished stage cannot advance"), Mode->CurrentWave, 3);
	// 테스트용 월드 정리. 실제 맵은 건드린 거 없음
	World->DestroyWorld(false);
	return true;
}

#endif
