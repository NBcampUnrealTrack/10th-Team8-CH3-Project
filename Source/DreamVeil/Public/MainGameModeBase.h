#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MainGameModeBase.generated.h"

class AActor;
class AMonsterBase;
class AMonsterSpawnVolume;
class USoundBase;
class UAudioComponent;

//웨이브가 바뀌었을 때 지금 웨이브와 전체 웨이브 수 HUD가 "3 / 6" 같은 표시에 씀
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnWaveChanged,
	int32, NewWave,
	int32, TotalWaves
);

//레벨이 끝나는 조건을 정함
//제한 시간 안에 몬스터를 다 잡으면 클리어해서 다음 레벨이 열림 시간을 넘기면 실패하고 로비로 돌아감
//보스가 있는 레벨(L4)은 보스를 잡는 순간 클리어 L4를 깨야 Endless가 열림
//로비와 메인 메뉴도 이 게임모드를 쓰지만 L1~L4가 아니면 아무것도 하지 않음
UCLASS()
class DREAMVEIL_API AMainGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMainGameModeBase();

	//스포너가 이번 레벨에 낼 몬스터를 다 냈을 때 부를 것
	//이 뒤로 살아있는 몬스터가 0이 되면 클리어 부르지 않으면 다 잡아도 클리어되지 않고 시간이 넘어가서 실패함
	UFUNCTION(BlueprintCallable, Category = "Level")
	void NotifyAllMonstersSpawned();
	// 현재 남은 시간
	UFUNCTION(BlueprintPure, Category = "Level")
	float GetLevelTimeRemaining() const;
	// 전체 제한 시간
	UFUNCTION(BlueprintPure, Category = "Level")
	float GetLevelTimeLimit() const;
	// 잠식 진행률 (0 ~ 1)
	UFUNCTION(BlueprintPure, Category = "Level")
	float GetLevelTimeProgress() const;

	// 잠식도 0 ~ 1
	//레벨 제한 시간과 별개임 시간이 지나면 차오르고 몬스터를 잡으면 내려가며 1에 닿으면 사망
	//값을 직접 들고 있어서 레벨과 Endless가 같은 규칙을 씀 모드별 처리가 필요 없음
	UFUNCTION(BlueprintPure, Category = "Corruption")
	float GetCorruption() const;

	// 잠식도 0 ~ 100 게이지 표시용
	UFUNCTION(BlueprintPure, Category = "Corruption")
	float GetCorruptionPercent() const;

	//잠식도를 내림 Amount는 0~1 단위 0.01이면 1퍼센트
	//0 아래로는 안 내려감 음수가 되면 게이지가 이상해짐
	UFUNCTION(BlueprintCallable, Category = "Corruption")
	void ReduceCorruption(float Amount);

	// 배경음
	// 맵마다 게임모드가 새로 만들어지므로 여기서 틀면 레벨이 바뀔 때 알아서 갈린다

	//로비에서 나오는 배경음
	//메인 메뉴 곡이 여기 없는 이유 메인 메뉴 맵은 엔진 기본 게임모드를 써서 이 클래스가 아예 돌지 않음
	//그쪽은 게임 인스턴스가 직접 틂 UDreamVeilGameInstance::PlayMainMenuBGM
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BGM")
	TObjectPtr<USoundBase> LobbyBGM;

	//레벨 1~4에서 나오는 배경음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BGM")
	TObjectPtr<USoundBase> LevelBGM;

	//보스가 살아 있는 동안 나오는 배경음 보스가 전부 죽으면 원래 곡으로 돌아감
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BGM")
	TObjectPtr<USoundBase> BossBGM;

	//Endless 전용 배경음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BGM")
	TObjectPtr<USoundBase> EndlessBGM;

	//게임 오버 화면에서 나오는 배경음 맞아 죽었을 때와 잠식됐을 때 둘 다 이걸 씀
	//비워두면 게임 오버 순간 배경음이 멈춤 조용한 것도 연출이라 그 자체는 고장이 아님
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BGM")
	TObjectPtr<USoundBase> GameOverBGM;

	//배경음을 갈아 끼움 같은 곡을 다시 넣으면 처음부터 되감지 않고 그대로 둠
	//비워서 부르면 배경음이 멈춤
	UFUNCTION(BlueprintCallable, Category = "BGM")
	void PlayBGM(USoundBase* NewBGM);

	//게임 오버 곡으로 갈아 끼움 컨트롤러가 게임 오버 화면을 띄울 때 부름
	//곡을 고르는 일을 컨트롤러가 아니라 여기서 하는 이유 배경음을 들고 있는 쪽이 게임모드라서
	UFUNCTION(BlueprintCallable, Category = "BGM")
	void PlayGameOverBGM();


	//보스 몬스터인지 보스 클래스가 아직 없어서 액터 태그 Boss로 구분
	//레벨 클리어 조건과 인벤토리 드롭이 같은 기준을 쓰도록 판정을 여기 하나만 둠
	static bool IsBossMonster(const AActor* Actor);

	// 웨이브

	//웨이브가 바뀔 때마다 알림 HUD가 받을 것
	UPROPERTY(BlueprintAssignable, Category = "Level|Wave")
	FOnWaveChanged OnWaveChanged;

	//지금 몇 번째 웨이브인지 아직 시작 전이면 0
	UFUNCTION(BlueprintPure, Category = "Level|Wave")
	int32 GetCurrentWave() const;

	//이 레벨의 전체 웨이브 수
	UFUNCTION(BlueprintPure, Category = "Level|Wave")
	int32 GetWaveCount() const;

protected:
	//레벨 제한 시간 초 앞 3분은 잡몹만 나오고 3분이 되면 보스가 나와서 남은 2분 안에 보스를 잡아야 클리어
	//시간을 넘기면 실패라 다음 레벨이 열리지 않음 블루프린트 게임모드에서 바꿀 수 있음
	//보스가 나오는 시각은 WaveInterval x WaveCount 라서 이 값만 늘리면 보스와 싸울 시간이 길어짐
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	float LevelTimeLimit = 300.0f;

	//웨이브 사이 간격 초 30초면 0 30 60 90 120 150초에 한 번씩 나옴
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Wave")
	float WaveInterval = 30.0f;

	//이 레벨의 전체 웨이브 수 마지막 웨이브까지 다 지나가면 그 순간 보스가 나옴
	//즉 보스 등장 시각이 곧 WaveInterval x WaveCount 30초 x 6이면 3분
	//따로 보스 등장 시각 변수를 두지 않는 이유 두 값이 어긋나면 잡몹이 끊긴 뒤에도 보스가 안 나오는 빈 시간이 생김
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Wave")
	int32 WaveCount = 6;

	//무한 모드에서 보스가 나오는 주기 초
	//레벨과 달리 끝이 없어서 웨이브가 아니라 시간으로 보스를 냄
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Boss")
	float EndlessBossInterval = 30.0f;

	//보스로 쓸 몬스터 웨이브가 다 지난 시각에 이걸 냄 레벨마다 다른 보스를 쓰려면 블루프린트에서 바꿀 것
	//기본값은 생성자에서 BP_BossMonster를 꽂아둠 비워두면 보스 없이 잡몹만 다 잡으면 클리어되는 예전 방식으로 돌아감
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Boss")
	TSubclassOf<AMonsterBase> BossClass;

	//보스를 플레이어 앞 얼마나 떨어진 곳에 낼지 보스 전용 스폰 지점이 생기면 안 써도 됨
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Boss")
	float BossSpawnDistance = 800.0f;

	virtual void BeginPlay() override;

private:
	//웨이브를 차례로 내보내는 타이머 마지막 웨이브를 내면 멈춤
	FTimerHandle WaveTimerHandle;

	//지금까지 내보낸 웨이브 수 아직 시작 전이면 0
	int32 CurrentWave = 0;

	//보스를 이미 냈는지 보스를 내고 나면 잡몹이 다 죽어도 선택지를 다시 묻지 않음
	bool bBossSpawned = false;

	//다음 웨이브를 내보냄 웨이브 타이머가 부름
	void StartNextWave();

	//맵에 있는 스폰 볼륨 전부에게 지금 내라고 신호를 보냄 몇 마리를 낼지는 볼륨이 정함
	//웨이브마다 몰아서 내지 않고 주기로 꾸준히 내보냄
	//웨이브는 몰아내기가 아니라 시간이 얼마나 지났는지 보여주는 눈금으로 남음
	void RequestContinuousSpawn();

	//다음 스폰 신호를 예약함 주기가 상황마다 달라서 반복 타이머 대신 한 번짜리를 매번 다시 검
	void ScheduleContinuousSpawn();

	//지금 레벨의 표 번호 L1이 0번 레벨 맵이 아니면 0번
	//상한과 주기가 같은 번호를 쓰게 한 곳에 둠 따로 계산하면 한쪽만 고치고 지나치기 쉬움
	int32 GetStageIndex() const;

	//지금 상황에서 동시에 살아 있을 수 있는 몬스터 수 레벨마다 다르고 Endless는 따로 둠
	int32 GetMaxAliveMonsters() const;

	//지금 상황에서 스폰 신호를 보내는 주기 초
	//뒤 레벨일수록 촘촘하고 Endless는 버틴 시간만큼 더 촘촘해짐
	float GetContinuousSpawnInterval() const;

	//무한 모드를 시작함 제한 시간과 웨이브가 없고 스폰과 보스만 주기로 돎
	void StartEndlessMode();

	//무한 모드에서 주기마다 보스를 냄 앞 보스가 아직 살아 있어도 또 냄
	void SpawnEndlessBoss();

	//스폰 볼륨에게 신호를 보내는 타이머 마지막 웨이브가 끝나면 멈춤
	FTimerHandle ContinuousSpawnTimerHandle;

	//무한 모드에서 보스를 내는 타이머
	FTimerHandle EndlessBossTimerHandle;

	//보스를 냄 실제로 나왔으면 true 보스 클래스가 비었거나 플레이어가 없으면 false
	//결과를 돌려주는 이유 보스가 못 나온 맵에서는 잡몹을 다 잡으면 클리어되는 예전 방식으로 돌아가야 해서
	bool SpawnBoss();

	//레벨 제한 시간 타이머 L1~L4에서만 걸림
	//타이머를 건 적이 없거나 레벨을 이미 끝냈으면 무효 상태라서 레벨이 두 번 끝나는 걸 막는 표시로도 씀
	FTimerHandle LevelTimerHandle;
	bool bTimedOut = false;

	//지금 잠식도 0~1 시간이 지나면 차오르고 몬스터를 잡으면 내려감
	float Corruption = 0.0f;

	//잠식도를 갱신하는 주기 초 촘촘할수록 게이지가 매끄럽고 부담은 커짐
	UPROPERTY(EditDefaultsOnly, Category = "Corruption", meta = (AllowPrivateAccess = "true"))
	float CorruptionTickInterval = 0.1f;

	//잠식도를 올리는 타이머 레벨과 Endless 양쪽에서 같이 걸림
	FTimerHandle CorruptionTimerHandle;

	//지금 상황에서 잠식도가 0에서 1까지 차오르는 데 걸리는 시간 초
	//한 마리도 안 잡고 가만히 있으면 이 시간 뒤에 죽는다는 뜻
	//뒤 레벨일수록 짧아지고 Endless는 버틴 시간만큼 더 짧아짐
	float GetCorruptionFillTime() const;

	//잠식도를 올리기 시작함 레벨과 Endless 준비가 끝난 뒤에 부름
	void StartCorruption();

	//주기마다 잠식도를 올리고 1에 닿으면 사망 처리
	void TickCorruption();

	//잠식도가 1에 닿음 멈출 타이머를 모드에 맞게 정리하고 실패 처리로 넘김
	void HandleCorruptionFull();

	//한 판이 실패로 끝났을 때의 공통 처리 게임 오버를 띄우고 로비로 보냄
	//제한 시간 초과와 잠식도 100퍼센트가 결과는 같아서 한 곳에 모음
	void HandleRunFailed();

	//지금 맵에 맞는 기본 배경음 로비 레벨 Endless 중 하나
	//보스 곡이 끝났을 때 무엇으로 돌아갈지도 이 값으로 정함
	USoundBase* GetBaseBGM() const;

	//살아 있는 보스가 있으면 보스 곡 없으면 기본 곡으로 맞춤
	//보스가 나올 때와 죽을 때 양쪽에서 불러서 판정이 한 곳에 모이게 함
	void UpdateBGMForBoss();

	//지금 돌고 있는 배경음 곡을 갈아 끼울 때 멈추려고 들고 있음
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BGMAudio;

	//지금 틀고 있는 곡 같은 곡을 다시 넣었을 때 되감지 않으려고 기억함
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> CurrentBGM;

	//지금 살아있는 몬스터 수
	int32 AliveMonsterCount = 0;

	//스포너가 낼 몬스터를 다 냈는지 이게 true여야 0마리일 때 클리어
	bool bAllMonstersSpawned = false;

	//몬스터의 사망 이벤트를 구독하고 살아있는 수에 더함
	void RegisterMonster(AMonsterBase* Monster);

	//월드에 액터가 스폰될 때마다 불림 몬스터면 등록
	void HandleActorSpawned(AActor* SpawnedActor);

	//등록한 몬스터가 죽었을 때
	UFUNCTION()
	void HandleMonsterDead();

	//보스가 죽었을 때 나머지 몬스터와 상관없이 바로 클리어
	UFUNCTION()
	void HandleBossDead();

	//스포너가 다 냈고 살아있는 몬스터가 없으면 클리어
	void TryClearLevel();

	//레벨 타이머를 멈춤 레벨 맵이 아니거나 이미 끝났으면 false 클리어와 실패가 같이 씀
	bool StopLevel();

	//플레이어가 죽었는지 죽었으면 클리어도 실패도 하지 않고 게임 오버 흐름에 맡김
	bool IsPlayerDead() const;

	//클리어 진행도를 올리고 로비로 보냄 다음 레벨이 열림
	void ClearLevel();

	//제한 시간 초과 진행도는 그대로 두고 로비로 보냄
	void FailLevel();
};
