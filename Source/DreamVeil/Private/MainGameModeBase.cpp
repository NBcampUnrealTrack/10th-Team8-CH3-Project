#include "MainGameModeBase.h"
#include "CombatStatsComponent.h"
#include "DreamVeilGameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "MonsterBase.h"
#include "TimerManager.h"

//보스 몬스터에 붙이는 액터 태그
//보스 클래스가 아직 없어서 태그로 구분함 보스 블루프린트의 Class Defaults > Actor > Tags에 Boss를 넣을 것
const FName BOSS_TAG = TEXT("Boss");

//레벨이 시작될 때
void AMainGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	//로비와 메인 메뉴도 이 게임모드를 쓰므로 L1~L4가 아니면 제한 시간을 걸지 않음
	//여기서 거르지 않으면 로비에서도 시간이 다 되면 클리어 처리돼서 진행도가 올라감
	UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>();

	if (!DreamVeilGameInstance || !DreamVeilGameInstance->IsInLevelMap())
	{
		return;
	}

	//제한 시간이 끝날 때까지 못 깨면 실패
	GetWorldTimerManager().SetTimer(LevelTimerHandle, this, &AMainGameModeBase::FailLevel, LevelTimeLimit, false);

	//맵에 미리 배치해둔 몬스터 등록
	for (TActorIterator<AMonsterBase> MonsterIterator(GetWorld()); MonsterIterator; ++MonsterIterator)
	{
		RegisterMonster(*MonsterIterator);
	}

	//앞으로 스폰될 몬스터도 등록 스포너 코드를 고치지 않고 게임모드가 알아서 셀 수 있게 월드의 스폰 알림을 받음
	//핸들러를 따로 해제하지 않는 이유 레벨이 바뀌면 월드와 함께 사라지고 CreateUObject라 게임모드가 먼저 사라져도 안전함
	GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &AMainGameModeBase::HandleActorSpawned));
}

//스포너가 이번 레벨에 낼 몬스터를 다 냈을 때
void AMainGameModeBase::NotifyAllMonstersSpawned()
{
	bAllMonstersSpawned = true;

	//마지막 몬스터가 알림보다 먼저 죽었을 수도 있으니 바로 한 번 확인
	TryClearLevel();
}

//몬스터의 사망 이벤트를 구독하고 살아있는 수에 더함
void AMainGameModeBase::RegisterMonster(AMonsterBase* Monster)
{
	if (!Monster)
	{
		return;
	}

	//몬스터 헤더의 변수 이름에 기대지 않고 컴포넌트로 찾음 몬스터 쪽 코드가 바뀌어도 여기는 안 고쳐도 됨
	UCombatStatsComponent* MonsterStats = Monster->FindComponentByClass<UCombatStatsComponent>();

	if (!MonsterStats || MonsterStats->IsDead())
	{
		return;
	}

	AliveMonsterCount++;

	MonsterStats->OnDead.AddDynamic(this, &AMainGameModeBase::HandleMonsterDead);

	//보스는 죽는 순간 따로 클리어 처리 L4는 보스를 잡아야 Endless가 열림
	//살아있는 수에서도 빠져야 하므로 위의 HandleMonsterDead 구독은 그대로 둠
	if (IsBossMonster(Monster))
	{
		MonsterStats->OnDead.AddDynamic(this, &AMainGameModeBase::HandleBossDead);
	}
}

//월드에 액터가 스폰될 때마다 불림
void AMainGameModeBase::HandleActorSpawned(AActor* SpawnedActor)
{
	//몬스터가 아니면 Cast가 nullptr를 돌려주고 RegisterMonster가 바로 돌아감
	RegisterMonster(Cast<AMonsterBase>(SpawnedActor));
}

//등록한 몬스터가 죽었을 때
void AMainGameModeBase::HandleMonsterDead()
{
	AliveMonsterCount = FMath::Max(AliveMonsterCount - 1, 0);

	TryClearLevel();
}

//보스가 죽었을 때
void AMainGameModeBase::HandleBossDead()
{
	ClearLevel();
}

//스포너가 다 냈고 살아있는 몬스터가 없으면 클리어
void AMainGameModeBase::TryClearLevel()
{
	//아직 더 나올 몬스터가 있으면 0마리여도 클리어가 아님 스폰 사이 빈틈에 레벨이 끝나버리는 걸 막음
	if (!bAllMonstersSpawned || AliveMonsterCount > 0)
	{
		return;
	}

	ClearLevel();
}

//레벨 타이머를 멈춤
bool AMainGameModeBase::StopLevel()
{
	//타이머를 건 적이 없는 맵(로비 메인 메뉴)이거나 이미 끝낸 레벨이면 false
	//시간 초과와 마지막 처치가 같은 프레임에 겹쳐도 레벨이 두 번 끝나지 않게 막음
	if (!LevelTimerHandle.IsValid())
	{
		return false;
	}

	//ClearTimer가 핸들을 무효로 만들어서 위 검사가 다음 호출을 막아줌 타이머 콜백 안에서 불러도 안전함
	GetWorldTimerManager().ClearTimer(LevelTimerHandle);

	return true;
}

//플레이어가 죽었는지
bool AMainGameModeBase::IsPlayerDead() const
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	UCombatStatsComponent* PlayerStats = PlayerPawn ? PlayerPawn->FindComponentByClass<UCombatStatsComponent>() : nullptr;

	return PlayerStats && PlayerStats->IsDead();
}

//클리어 다음 레벨이 열림
void AMainGameModeBase::ClearLevel()
{
	if (!StopLevel())
	{
		return;
	}

	//타이머를 먼저 멈추고 확인하는 이유 죽은 뒤에 독 데미지로 마지막 몬스터가 죽어도 클리어되지 않게
	if (IsPlayerDead())
	{
		return;
	}

	UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>();

	if (!DreamVeilGameInstance)
	{
		return;
	}

	//증강 저장 진행도 올리기 로비 이동은 GameInstance가 한 번에 함
	DreamVeilGameInstance->CompleteCurrentLevel();
}

//제한 시간 초과
void AMainGameModeBase::FailLevel()
{
	if (!StopLevel())
	{
		return;
	}

	//죽은 뒤에 시간이 끝났으면 로비로 보내지 않음 게임 오버 쪽 흐름과 겹치지 않게
	if (IsPlayerDead())
	{
		return;
	}

	UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>();

	if (!DreamVeilGameInstance)
	{
		return;
	}

	DreamVeilGameInstance->FailCurrentLevel();
}

//보스 몬스터인지
bool AMainGameModeBase::IsBossMonster(const AActor* Actor)
{
	return Actor && Actor->ActorHasTag(BOSS_TAG);
}
