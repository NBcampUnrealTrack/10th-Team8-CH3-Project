#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "StageCombatProperties.h"
#include "MainGameModeBase.generated.h"

class AMonsterBase;
class AMonsterSpawnVolume;

//웨이브 번호 없이 현재 전투 상태만 구분함. 기존 BP에 저장된 숫자는 유지함
UENUM(BlueprintType)
enum class EStagePhase : uint8
{
	Combat = 0,
	Boss = 2,
	Finished = 3
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStagePhaseChanged, EStagePhase, NewPhase);

//게임모드는 시간과 소환 예약을 관리하고 실제 위치 선정과 생성은 볼륨에 맡김
UCLASS()
class DREAMVEIL_API AMainGameModeBase : public AGameModeBase
{
	GENERATED_BODY()
public:
	//보스전 안내 UI는 웨이브 번호 대신 상태 변경을 받아서 표시함
	UPROPERTY(BlueprintAssignable, Category = "Level|Stage")
	FOnStagePhaseChanged OnStagePhaseChanged;

	UFUNCTION(BlueprintPure, Category = "Level|Stage")
	EStagePhase GetStagePhase() const { return StagePhase; }
	//Tick으로 시간을 더하지 않고 월드 게임 시간을 사용해서 일시정지와 시간 배율을 따름
	UFUNCTION(BlueprintPure, Category = "Level")
	float GetLevelElapsedTime() const;
	//기존 잠식 UI의 연결을 유지하되 이제 웨이브가 아니라 보스 등장까지 남은 시간을 반환함
	UFUNCTION(BlueprintPure, Category = "Level")
	float GetPhaseTimeRemaining() const;
	UFUNCTION(BlueprintPure, Category = "Level")
	float GetLevelTimeRemaining() const;
	UFUNCTION(BlueprintPure, Category = "Level")
	float GetLevelTimeLimit() const;
	UFUNCTION(BlueprintPure, Category = "Level")
	float GetLevelTimeProgress() const;

	//보스 판정은 드롭에서도 사용하므로 기존 Boss 태그 규칙을 유지함
	static bool IsBossMonster(const AActor* Actor);

protected:
	//맵별 게임모드 BP에서 수량, 증가 주기/증가량, 소환 간격과 보스 시간을 조절함
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Stage")
	FStageCombatProperties StageProperties;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Boss", meta = (ClampMin = "0"))
	float BossSpawnDistance = 800.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	bool bUseLevelTimeLimit = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level", meta = (EditCondition = "bUseLevelTimeLimit", ClampMin = "0.1"))
	float LevelTimeLimit = 180.0f;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class FStageTimedSpawnTest;
	EStagePhase StagePhase = EStagePhase::Combat;
	bool bStageActive = false;
	//맵 시작점을 저장하여 다른 맵에서 흐른 시간이 섞이지 않게 함
	double LevelStartTime = 0.0;
	//이전 예약이 남았으면 뒤에 더함. 처치 수와는 무관함
	int32 PendingSpawnCount = 0;
	FTimerHandle SpawnIncreaseTimerHandle;
	FTimerHandle MonsterSpawnTimerHandle;
	FTimerHandle BossTimerHandle;
	FTimerHandle LevelTimerHandle;
	//약한 참조로 레벨에서 제거된 볼륨이나 보스의 수명을 붙잡지 않음
	TArray<TWeakObjectPtr<AMonsterSpawnVolume>> SpawnVolumes;
	TWeakObjectPtr<AMonsterBase> StageBoss;

	void SetStagePhase(EStagePhase NewPhase);
	void StartStage();
	void RequestMonsterSpawn();
	void SpawnNextMonster();
	void StartBossFight();
	void CompleteStage();
	void StopStage();
	void StopStageTimers();
	void FailLevel();
	bool IsPlayerDead() const;
	//플레이어가 죽으면 즉시 예약을 취소하고 로비 복귀는 기존 게임 오버 UI에 맡김
	UFUNCTION()
	void HandlePlayerDead();
	UFUNCTION()
	void HandleBossDead();
};

//이전 구조 학습 메모: 아래 주석은 원문 보존용이며 현재 구현 설명이 아님
// 지금 일반 전투인지 보스전인지. 준비 상태는 따로 안 두고 첫 웨이브 바로 시작함
// 일반 몬스터 나오는 구간. 정예도 여기 포함임
// 마지막 웨이브. 얜 보스 잡아야 끝남
// 클리어든 실패든 더 진행 안 하는 상태
// HUD에서 2 / 6 같은 웨이브 번호 띄울 때 쓰세요
// 얜 번호 말고 전투 / 보스 / 종료 상태 바뀐 거 알려줌
// 이번에 몇 마리 낼지랑 설정 전달용. 실제 몬스터 만드는 건 스포너에서 구현해야 됨
// 얜 잠깐 멈추는 게 아니라 남은 생성 요청 전부 취소하라는 알림
// 예전 보스 선택 UI 안 깨지게 남겨둠. 지금은 자동 등장이라 이 알림 안 보냄
// 스테이지 진행 담당. 일반 스폰은 요청만 보내고 웨이브 넘어가는 조건을 여기서 봄
// 실제로 다음 웨이브 시작할 때 보냄.
// 보스전 안내 같은 건 이거 받아서 바꾸세요
// 스포너 연결할 자리. 받은 요청 다 처리했으면 그 ID로 NotifySpawnRequestFinished 호출 ㄱㄱ
// 맵 끝났으니 생성 예약도 치우라는 알림
// 구형 BP 연결 보존용. 새 UI는 OnStagePhaseChanged 쓰세요
// 현재 웨이브 번호. 시작 전은 0이고 첫 전투부터 1
// 보스 웨이브까지 포함한 전체 횟수
// 지금 일반 전투인지 보스전인지 확인할 때 쓰는 거
// 지금 전투가 몇 초 남았는지. 보스전이랑 끝난 상태는 0 나옴
// 얜 스테이지 전체 제한 시간 쪽. 위의 웨이브 시간하고 별개임
// 전체 제한 시간 꺼뒀으면 0. HUD에서도 그때는 타이머 안 띄우면 됨
// 전체 제한 시간을 얼마나 썼는지 0~1로 줌. 잠식 표시용
// 보스 판정은 Boss 태그로 통일. 드롭 쪽에서도 이거 씀
// 받은 요청 ID 하나를 다 처리했을 때 호출. 예약만 걸어놓고 부르면 안 됨
// 예전 BP 노드 남겨둔 거. 이걸 불러도 대기 중인 요청이 있으면 못 넘어감
// 예전 도전 버튼 연결용 빈 함수. 이제 마지막 웨이브에서 알아서 보스 나옴
// 예전 돌아가기 버튼도 호출만 받아줌. 보스 안 잡고 클리어하는 건 막음
// 맵별 전투 설정은 여서 하세요. 엔드리스는 총 3회에 일반 수량 2칸 넣으면 됨
// 한 마리씩 내는 간격. 게임모드는 이 값만 전달하고 실제 생성은 스포너 담당
// 보스를 플레이어 앞 얼마나 떨어진 곳에 낼지. 일단 거리 기준으로 둠
// 전체 제한 시간 쓸 거면 이거 켜세요. 일단 기본은 꺼둠
// 위 옵션 켰을 때만 쓰는 초 단위 제한. 보스전 시간도 포함됨
// 설정 확인하고 첫 웨이브 바로 시작
// 맵 나갈 때 타이머랑 액터 생성 알림 연결 정리
// 테스트에서 내부 상태 확인하려고 열어둔 거. 게임 코드에서 쓸 용도는 아님
// 현재 진행 상태. 시작 전에는 bStageActive가 false라 아직 안 돌아감
// 지금 몇 번째 웨이브인지.
// 로비거나 이미 끝난 판에서 진행 함수 또 도는 거 막는 용도
// 시작 중 완료 답이 와도 재귀로 다음 웨이브 열지 않게 막음
// 일반 전투 시간 다 되면 다음 웨이브로 넘겨줌
// 스테이지 전체 제한 시간. 옵션 꺼져 있으면 안 걸림
// 월드의 액터 생성 알림 구독한 번호. 맵 나갈 때 이걸로 해제함
// 정예랑 이전 웨이브 잔여몹도 포함. 보스는 여기서 안 셈
// 요청할 때마다 올림. 완료 알림이 어느 웨이브 건지 구분하려고 둠
// 아직 처리 끝났다는 답 안 온 요청들. 비어 있어야 조기 전멸 인정됨
// 같은 몬스터 두 번 세면 안 되니까 등록한 애들은 기억해둠
// 이번에 만든 보스 기록. 약한 참조라 보스가 없어지는 걸 붙잡지는 않음
// 상태 바꿀 땐 이걸 거치면 됨. UI 알림도 같이 나감
// 번호 하나 올리고 일반 전투인지 마지막 보스인지 나눔
// 전투 타이머 걸고 이번 웨이브 생성 요청
// 마지막 웨이브. 보스 사망 알림 연결하고 등장시킴
// 수량이랑 스테이지 설정 전달만 함. 여기서 일반 몬스터 직접 만드는 거 아님
// 시간 다 됐으니 남은 몬스터 있어도 다음 웨이브 시작
// 생성 대기도 없고 살아 있는 몬스터도 없으면 남은 시간 스킵
// 보스 잡았을 때 판 정리하고 GameInstance에 클리어 전달
// 타이머 전부 끄고 스폰 대기 요청도 취소하라고 알림
// 전체 제한 시간 넘겼을 때 실패 처리
// 플레이어 죽었으면 게임 오버 쪽에 맡기려고 체크함
// 살아 있는 수에 더하고 사망 알림 연결. 이미 센 애면 무시함
// 월드에 뭐가 생겼든 일단 몬스터인지 확인
// 일반 / 정예 한 마리 죽을 때마다 수 줄이고 전멸인지 확인
// 보스는 죽으면 바로 스테이지 클리어
