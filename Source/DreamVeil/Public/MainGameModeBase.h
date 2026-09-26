#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StageCombatProperties.h"
#include "MainGameModeBase.generated.h"

class AMonsterBase;

// 지금 싸우는 중인지 쉬는 중인지. 준비 상태는 따로 안 두고 첫 웨이브 바로 시작함
UENUM(BlueprintType)
enum class EStagePhase : uint8
{
	// 일반 몬스터 나오는 구간. 정예도 여기 포함임
	NormalWave,
	// 다음 웨이브 전 쉬는 시간. 남은 몬스터까지 멈추는 건 아님
	Rest,
	// 마지막 웨이브. 얜 보스 잡아야 끝남
	BossWave,
	// 클리어든 실패든 더 진행 안 하는 상태
	Finished
};

// HUD에서 2 / 6 같은 웨이브 번호 띄울 때 쓰세요
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWaveChanged, int32, NewWave, int32, TotalWaves);
// 얜 번호 말고 전투 / 휴식 / 보스 / 종료 상태 바뀐 거 알려줌
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStagePhaseChanged, EStagePhase, NewPhase);
// 이번에 몇 마리 낼지랑 설정 전달용. 실제 몬스터 만드는 건 스포너에서 구현해야 됨
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnWaveSpawnRequested, int32, RequestId, int32, MonsterCount, float, SpawnInterval, const FStageCombatProperties&, Properties);
// true면 생성 잠깐 멈추고 false면 다시 이어가라는 뜻
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaveSpawningPausedChanged, bool, bPaused);
// 얜 잠깐 멈추는 게 아니라 남은 생성 요청 전부 취소하라는 알림
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWaveSpawningCancelled);
// 예전 보스 선택 UI 안 깨지게 남겨둠. 지금은 자동 등장이라 이 알림 안 보냄
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBossChoiceReady);

// 스테이지 진행 담당. 일반 스폰은 요청만 보내고 웨이브 넘어가는 조건을 여기서 봄
UCLASS()
class DREAMVEIL_API AMainGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
public:
	// 실제로 다음 웨이브 시작할 때 보냄. 휴식 들어갈 때 번호 올리면 안 됨
	UPROPERTY(BlueprintAssignable, Category = "Level|Wave")
	FOnWaveChanged OnWaveChanged;
	// 휴식 안내 같은 건 이거 받아서 바꾸세요
	UPROPERTY(BlueprintAssignable, Category = "Level|Wave")
	FOnStagePhaseChanged OnStagePhaseChanged;
	// 스포너 연결할 자리. 받은 요청 다 처리했으면 그 ID로 NotifySpawnRequestFinished 호출 ㄱㄱ
	UPROPERTY(BlueprintAssignable, Category = "Level|Spawn")
	FOnWaveSpawnRequested OnWaveSpawnRequested;
	// 쉬는 동안 추가 생성만 멈추라는 알림. 실제 정지는 스포너 쪽에서 해줘야 됨
	UPROPERTY(BlueprintAssignable, Category = "Level|Spawn")
	FOnWaveSpawningPausedChanged OnWaveSpawningPausedChanged;
	// 맵 끝났으니 생성 예약도 치우라는 알림
	UPROPERTY(BlueprintAssignable, Category = "Level|Spawn")
	FOnWaveSpawningCancelled OnWaveSpawningCancelled;
	// 구형 BP 연결 보존용. 새 UI는 OnStagePhaseChanged 쓰세요
	UPROPERTY(BlueprintAssignable, Category = "Level|Boss", meta = (DeprecatedProperty, DeprecationMessage = "Boss waves start automatically."))
	FOnBossChoiceReady OnBossChoiceReady;

	// 현재 웨이브 번호. 시작 전은 0이고 첫 전투부터 1
	UFUNCTION(BlueprintPure, Category = "Level|Wave")
	int32 GetCurrentWave() const { return CurrentWave; }
	// 보스 웨이브까지 포함한 전체 횟수
	UFUNCTION(BlueprintPure, Category = "Level|Wave")
	int32 GetWaveCount() const { return StageProperties.TotalWaves; }
	// 지금 전투인지 휴식인지 확인할 때 쓰는 거
	UFUNCTION(BlueprintPure, Category = "Level|Wave")
	EStagePhase GetStagePhase() const { return StagePhase; }
	// 지금 전투나 휴식이 몇 초 남았는지. 보스전이랑 끝난 상태는 0 나옴
	UFUNCTION(BlueprintPure, Category = "Level|Wave")
	float GetPhaseTimeRemaining() const;
	// 얜 스테이지 전체 제한 시간 쪽. 위의 웨이브 시간하고 별개임
	UFUNCTION(BlueprintPure, Category = "Level")
	float GetLevelTimeRemaining() const;
	// 전체 제한 시간 꺼뒀으면 0. HUD에서도 그때는 타이머 안 띄우면 됨
	UFUNCTION(BlueprintPure, Category = "Level")
	float GetLevelTimeLimit() const;
	// 전체 제한 시간을 얼마나 썼는지 0~1로 줌. 잠식 표시용
	UFUNCTION(BlueprintPure, Category = "Level")
	float GetLevelTimeProgress() const;

	// 보스 판정은 Boss 태그로 통일. 드롭 쪽에서도 이거 씀
	static bool IsBossMonster(const AActor* Actor);
	// 받은 요청 ID 하나를 다 처리했을 때 호출. 예약만 걸어놓고 부르면 안 됨
	UFUNCTION(BlueprintCallable, Category = "Level|Spawn")
	void NotifySpawnRequestFinished(int32 RequestId);
	// 예전 BP 노드 남겨둔 거. 이걸 불러도 대기 중인 요청이 있으면 못 넘어감
	UFUNCTION(BlueprintCallable, Category = "Level", meta = (DeprecatedFunction, DeprecationMessage = "Spawners report completion per request automatically."))
	void NotifyAllMonstersSpawned();
	// 예전 도전 버튼 연결용 빈 함수. 이제 마지막 웨이브에서 알아서 보스 나옴
	UFUNCTION(BlueprintCallable, Category = "Level|Boss", meta = (DeprecatedFunction, DeprecationMessage = "Boss waves start automatically."))
	void AcceptBossChallenge() {}
	// 예전 돌아가기 버튼도 호출만 받아줌. 보스 안 잡고 클리어하는 건 막음
	UFUNCTION(BlueprintCallable, Category = "Level|Boss", meta = (DeprecatedFunction, DeprecationMessage = "Defeat the boss to complete the stage."))
	void DeclineBossChallenge() {}

protected:
	// 맵별 전투 설정은 여서 하세요. 엔드리스는 총 3회에 일반 수량 2칸 넣으면 됨
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Stage")
	FStageCombatProperties StageProperties;
	// 한 마리씩 내는 간격. 게임모드는 이 값만 전달하고 실제 생성은 스포너 담당
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Wave", meta = (ClampMin = "0.05"))
	float WaveSpawnInterval = 0.5f;
	// 보스를 플레이어 앞 얼마나 떨어진 곳에 낼지. 일단 거리 기준으로 둠
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Boss")
	float BossSpawnDistance = 800.0f;
	// 전체 제한 시간 쓸 거면 이거 켜세요. 일단 기본은 꺼둠
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	bool bUseLevelTimeLimit = false;
	// 위 옵션 켰을 때만 쓰는 초 단위 제한. 휴식이랑 보스전 시간도 포함됨
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level", meta = (EditCondition = "bUseLevelTimeLimit", ClampMin = "0.1"))
	float LevelTimeLimit = 180.0f;
	// 설정 확인하고 첫 웨이브 바로 시작
	virtual void BeginPlay() override;
	// 맵 나갈 때 타이머랑 액터 생성 알림 연결 정리
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	// 테스트에서 내부 상태 확인하려고 열어둔 거. 게임 코드에서 쓸 용도는 아님
	friend class FStageWaveFlowTest;
	// 현재 진행 상태. 시작 전에는 bStageActive가 false라 아직 안 돌아감
	EStagePhase StagePhase = EStagePhase::NormalWave;
	// 지금 몇 번째 웨이브인지. 휴식 중에는 방금 끝난 번호 유지함
	int32 CurrentWave = 0;
	// 로비거나 이미 끝난 판에서 진행 함수 또 도는 거 막는 용도
	bool bStageActive = false;

	// 일반 전투 시간 다 되면 휴식으로 넘겨줌
	FTimerHandle WaveTimerHandle;
	// 쉬는 시간 끝나면 다음 웨이브 부름
	FTimerHandle RestTimerHandle;
	// 스테이지 전체 제한 시간. 옵션 꺼져 있으면 안 걸림
	FTimerHandle LevelTimerHandle;

	// 월드의 액터 생성 알림 구독한 번호. 맵 나갈 때 이걸로 해제함
	FDelegateHandle ActorSpawnedHandle;

	// 정예랑 이전 웨이브 잔여몹도 포함. 보스는 여기서 안 셈
	int32 AliveNormalMonsterCount = 0;
	// 요청할 때마다 올림. 완료 알림이 어느 웨이브 건지 구분하려고 둠
	int32 NextSpawnRequestId = 0;

	// 아직 처리 끝났다는 답 안 온 요청들. 비어 있어야 조기 전멸 인정됨
	TSet<int32> PendingSpawnRequests;
	// 같은 몬스터 두 번 세면 안 되니까 등록한 애들은 기억해둠
	TSet<TWeakObjectPtr<AMonsterBase>> RegisteredMonsters;

	// 이번에 만든 보스 기록. 약한 참조라 보스가 없어지는 걸 붙잡지는 않음
	TWeakObjectPtr<AMonsterBase> StageBoss;

	// 상태 바꿀 땐 이걸 거치면 됨. UI 알림도 같이 나감
	void SetStagePhase(EStagePhase NewPhase);
	// 번호 하나 올리고 일반 전투인지 마지막 보스인지 나눔
	void StartNextWave();
	// 전투 타이머 걸고 이번 웨이브 생성 요청
	void StartNormalWave();
	// 마지막 웨이브. 보스 사망 알림 연결하고 등장시킴
	void StartBossWave();
	// 수량이랑 스테이지 설정 전달만 함. 여기서 일반 몬스터 직접 만드는 거 아님
	void RequestWaveSpawn();
	// 시간 다 됐으니 남은 몬스터 있어도 휴식 시작
	void OnWaveTimeExpired();
	// 생성 대기도 없고 살아 있는 몬스터도 없으면 남은 시간 스킵
	void TryFinishNormalWaveEarly();
	// 전투 타이머 끄고 휴식 타이머 켬
	void BeginRest();
	// 아직 휴식 상태일 때만 다음 웨이브로 넘김
	void OnRestFinished();
	// 보스 잡았을 때 판 정리하고 GameInstance에 클리어 전달
	void CompleteStage();
	// 타이머 전부 끄고 스폰 대기 요청도 취소하라고 알림
	void StopStageTimers();
	// 전체 제한 시간 넘겼을 때 실패 처리
	void FailLevel();
	// 플레이어 죽었으면 게임 오버 쪽에 맡기려고 체크함
	bool IsPlayerDead() const;

	// 살아 있는 수에 더하고 사망 알림 연결. 이미 센 애면 무시함
	void RegisterMonster(AMonsterBase* Monster);
	// 월드에 뭐가 생겼든 일단 몬스터인지 확인
	void HandleActorSpawned(AActor* SpawnedActor);
	// 일반 / 정예 한 마리 죽을 때마다 수 줄이고 전멸인지 확인
	UFUNCTION()
	void HandleMonsterDead();
	// 보스는 죽으면 바로 스테이지 클리어
	UFUNCTION()
	void HandleBossDead();
};
