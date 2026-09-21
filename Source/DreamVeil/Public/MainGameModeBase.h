#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MainGameModeBase.generated.h"

class AActor;
class AMonsterBase;


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

protected:
	//레벨 제한 시간 초 이 안에 다 잡아야 클리어 넘기면 실패 블루프린트 게임모드에서 바꿀 수 있음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	float LevelTimeLimit = 180.0f;

	virtual void BeginPlay() override;

private:
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
