#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MainGameModeBase.generated.h"

class AActor;
class AMonsterBase;
class AMonsterSpawnVolume;

//웨이브가 바뀌었을 때 지금 웨이브와 전체 웨이브 수 HUD가 "3 / 6" 같은 표시에 씀
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnWaveChanged,
	int32, NewWave,
	int32, TotalWaves
);

//마지막 레벨에서 웨이브를 다 넘기고 몬스터도 다 잡았을 때 한 번
//UI는 이걸 받아서 보스에 도전할지 로비로 돌아갈지 묻는 창을 띄울 것
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBossChoiceReady);


//레벨이 끝나는 조건을 정함
//제한 시간 안에 몬스터를 다 잡으면 클리어해서 다음 레벨이 열림 시간을 넘기면 실패하고 로비로 돌아감
//보스가 있는 레벨(L4)은 보스를 잡는 순간 클리어 L4를 깨야 Endless가 열림
//로비와 메인 메뉴도 이 게임모드를 쓰지만 L1~L4가 아니면 아무것도 하지 않음
UCLASS()
class DREAMVEIL_API AMainGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
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

	//보스 몬스터인지 보스 클래스가 아직 없어서 액터 태그 Boss로 구분
	//레벨 클리어 조건과 인벤토리 드롭이 같은 기준을 쓰도록 판정을 여기 하나만 둠
	static bool IsBossMonster(const AActor* Actor);

	// 웨이브

	//웨이브가 바뀔 때마다 알림 HUD가 받을 것
	UPROPERTY(BlueprintAssignable, Category = "Level|Wave")
	FOnWaveChanged OnWaveChanged;

	//마지막 레벨에서 보스 도전 여부를 물어야 할 때 알림
	UPROPERTY(BlueprintAssignable, Category = "Level|Boss")
	FOnBossChoiceReady OnBossChoiceReady;

	//지금 몇 번째 웨이브인지 아직 시작 전이면 0
	UFUNCTION(BlueprintPure, Category = "Level|Wave")
	int32 GetCurrentWave() const;

	//이 레벨의 전체 웨이브 수
	UFUNCTION(BlueprintPure, Category = "Level|Wave")
	int32 GetWaveCount() const;

	//보스에 도전 보스 선택 UI의 도전 버튼이 부를 것
	//보스를 내고 웨이브도 계속 돌려서 보스전 중에도 잡몹이 나옴
	UFUNCTION(BlueprintCallable, Category = "Level|Boss")
	void AcceptBossChallenge();

	//보스를 넘기고 로비로 보스 선택 UI의 돌아가기 버튼이 부를 것
	UFUNCTION(BlueprintCallable, Category = "Level|Boss")
	void DeclineBossChallenge();

protected:
	//레벨 제한 시간 초 이 안에 다 잡아야 클리어 넘기면 실패 블루프린트 게임모드에서 바꿀 수 있음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	float LevelTimeLimit = 180.0f;

	//웨이브 사이 간격 초 30초면 0 30 60 90 120 150초에 한 번씩 나옴
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Wave")
	float WaveInterval = 30.0f;

	//이 레벨의 전체 웨이브 수 마지막 웨이브를 내고 나면 더 안 나옴
	//제한 시간과 맞추려면 WaveInterval x WaveCount 가 LevelTimeLimit 이하여야 함
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Wave")
	int32 WaveCount = 6;

	//웨이브 한 번에 스폰 볼륨 하나가 낼 몬스터 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Wave")
	int32 MonstersPerWave = 4;

	//한 마리씩 낼 때 사이 간격 초 0.5면 4마리가 1.5초에 걸쳐 나옴
	//한꺼번에 쏟아지지 않게 끊어 내려는 것
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Wave")
	float WaveSpawnInterval = 0.5f;

	//스폰 볼륨에게 내라고 신호를 보내는 주기 초
	//웨이브마다 몰아서 내지 않고 이 주기로 꾸준히 내보냄 몇 마리를 낼지는 볼륨이 정함
	//웨이브는 몰아내기가 아니라 시간이 얼마나 지났는지 보여주는 눈금으로 남음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Wave")
	float ContinuousSpawnInterval = 3.0f;

	//무한 모드에서 보스가 나오는 주기 초
	//레벨과 달리 끝이 없어서 웨이브가 아니라 시간으로 보스를 냄
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Boss")
	float EndlessBossInterval = 30.0f;

	//보스로 쓸 몬스터 마지막 레벨에서 도전을 고르면 이걸 냄
	//비워두면 보스 선택지 자체가 뜨지 않고 웨이브를 다 넘긴 순간 바로 클리어됨
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

	//보스 도전 여부를 묻는 창이 떠 있는지 답을 두 번 받지 않으려고 둠
	bool bBossChoicePending = false;

	//보스를 이미 냈는지 보스를 내고 나면 잡몹이 다 죽어도 선택지를 다시 묻지 않음
	bool bBossSpawned = false;

	//다음 웨이브를 내보냄 웨이브 타이머가 부름
	void StartNextWave();

	//맵에 있는 스폰 볼륨 전부에게 지금 내라고 신호를 보냄 몇 마리를 낼지는 볼륨이 정함
	void RequestContinuousSpawn();

	//무한 모드를 시작함 제한 시간과 웨이브가 없고 스폰과 보스만 주기로 돎
	void StartEndlessMode();

	//무한 모드에서 주기마다 보스를 냄 앞 보스가 아직 살아 있어도 또 냄
	void SpawnEndlessBoss();

	//스폰 볼륨에게 신호를 보내는 타이머 마지막 웨이브가 끝나면 멈춤
	FTimerHandle ContinuousSpawnTimerHandle;

	//무한 모드에서 보스를 내는 타이머
	FTimerHandle EndlessBossTimerHandle;

	//보스를 냄 도전을 고른 뒤에 불림
	void SpawnBoss();

	//지금 맵이 마지막 레벨(보스가 있는 곳)인지
	bool IsFinalLevel() const;

	//레벨 제한 시간 타이머 L1~L4에서만 걸림
	//타이머를 건 적이 없거나 레벨을 이미 끝냈으면 무효 상태라서 레벨이 두 번 끝나는 걸 막는 표시로도 씀
	FTimerHandle LevelTimerHandle;

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
