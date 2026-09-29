#include "MainGameModeBase.h"

#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "MainPlayerController.h"
#include "CombatStatsComponent.h"
#include "DispatchTableComponent.h"
#include "DreamVeilGameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "MonsterBase.h"
#include "MonsterSpawnVolume.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

//보스 몬스터에 붙이는 액터 태그
//보스 클래스가 아직 없어서 태그로 구분함 보스 블루프린트의 Class Defaults > Actor > Tags에 Boss를 넣을 것
const FName BOSS_TAG = TEXT("Boss");

//스폰 밸런스 여기 숫자만 바꾸면 조절됨
//레벨 순서 L1 L2 L3 L4

//레벨마다 동시에 살아 있을 수 있는 몬스터 수 이 수에 닿으면 잡아서 자리가 날 때까지 새로 내지 않음
//볼륨을 문마다 놓으면 주기마다 볼륨 수만큼 나와서 5분이면 수백 마리가 됨
//볼륨이 아니라 게임모드가 세는 이유 볼륨은 자기가 낸 수만 알지 판 전체에 몇 마리가 있는지 모름
const int32 MAX_ALIVE_MONSTERS_BY_STAGE[] = { 40, 50, 60, 70 };

//Endless에서 동시에 살아 있을 수 있는 몬스터 수
//레벨보다 높게 둔 이유 끝이 없어서 오래 버틸수록 촘촘해지는데 상한이 낮으면 후반이 오히려 한산해짐
const int32 ENDLESS_MAX_ALIVE_MONSTERS = 100;

//레벨마다 스폰 볼륨에게 내라고 신호를 보내는 주기 초 뒤 레벨일수록 짧아져서 더 빨리 나옴
const float SPAWN_INTERVAL_BY_STAGE[] = { 3.0f, 2.5f, 2.0f, 1.5f };

//Endless에서 1분이 지날 때마다 스폰 주기가 줄어드는 초
//시작 주기는 마지막 레벨 값을 그대로 씀 L4를 깨고 이어지는 곳이라 숫자를 따로 두지 않음
const float ENDLESS_SPAWN_INTERVAL_DECAY_PER_MINUTE = 0.15f;

//Endless에서 아무리 오래 버텨도 이보다 짧아지지는 않음
//0에 가까워지면 타이머가 거의 매 프레임 돌면서 스폰이 폭주해 게임이 멈춤
const float ENDLESS_MIN_SPAWN_INTERVAL = 0.5f;

//레벨마다 잠식도가 0에서 100퍼센트까지 차오르는 데 걸리는 시간 초
//한 마리도 안 잡고 가만히 있으면 이 시간 뒤에 죽는다는 뜻 뒤 레벨일수록 짧아서 더 바쁘게 잡아야 함
//제한 시간(5분 300초)보다 짧게 둔 이유 시간만 버티면 되는 게 아니라 계속 잡아야 하게 만들려는 것
const float CORRUPTION_FILL_TIME_BY_STAGE[] = { 270.0f, 240.0f, 210.0f, 180.0f };

//Endless에서 1분이 지날 때마다 잠식도 차오르는 시간이 줄어드는 초
//시작 값은 마지막 레벨과 같음 L4를 깨고 이어지는 곳이라 숫자를 따로 두지 않음
const float ENDLESS_CORRUPTION_FILL_DECAY_PER_MINUTE = 15.0f;

//Endless에서 아무리 오래 버텨도 이보다 빨리 차지는 않음
//끝없이 빨라지면 잡는 속도로는 절대 못 따라가는 구간이 생겨서 실력과 상관없이 끝남
const float ENDLESS_MIN_CORRUPTION_FILL_TIME = 90.0f;

//레벨별 표의 칸 수가 어긋나면 컴파일에서 바로 걸리게 막음 한쪽만 레벨을 늘리면 다른 쪽이 마지막 값으로 눌림
static_assert(UE_ARRAY_COUNT(MAX_ALIVE_MONSTERS_BY_STAGE) == UE_ARRAY_COUNT(SPAWN_INTERVAL_BY_STAGE), "spawn tables need one value per level");
static_assert(UE_ARRAY_COUNT(MAX_ALIVE_MONSTERS_BY_STAGE) == UE_ARRAY_COUNT(CORRUPTION_FILL_TIME_BY_STAGE), "corruption table needs one value per level");

//보스 클래스 기본값을 꽂아둠
//블루프린트에서 채워도 되지만 안 채우면 보스가 아예 안 나와서 레벨이 끝나지 않으므로 코드가 기본값을 들고 있음
//BedActor ComputerActor가 메시를 찾는 것과 같은 방식 블루프린트에서 다른 보스로 바꾸면 그쪽이 이김
AMainGameModeBase::AMainGameModeBase()
{
	static ConstructorHelpers::FClassFinder<AMonsterBase>
		BossClassFinder(TEXT("/Game/Blueprint/Monsters/BP_BossMonster"));

	if (BossClassFinder.Succeeded())
	{
		BossClass = BossClassFinder.Class;
	}
}

//레벨이 시작될 때
void AMainGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	//로비와 메인 메뉴도 이 게임모드를 쓰므로 L1~L4가 아니면 제한 시간을 걸지 않음
	//여기서 거르지 않으면 로비에서도 시간이 다 되면 클리어 처리돼서 진행도가 올라감
	UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>();

	if (!DreamVeilGameInstance)
	{
		return;
	}

	//무한 모드는 제한 시간도 웨이브도 없어서 완전히 다른 흐름을 탐
	//아래 레벨용 준비를 같이 하면 시간이 다 됐을 때 실패 처리가 돌아서 무한 모드가 끝나버림
	//맵에 맞는 배경음부터 틀어둠 로비 메인메뉴 레벨 Endless가 각각 다름
	//아래 갈래마다 따로 틀지 않는 이유 로비와 메인메뉴는 중간에 return으로 빠져나감
	PlayBGM(GetBaseBGM());

	if (DreamVeilGameInstance->IsInEndless())
	{
		StartEndlessMode();
		return;
	}

	//로비와 메인 메뉴도 이 게임모드를 쓰므로 L1~L4가 아니면 아무것도 하지 않음
	if (!DreamVeilGameInstance->IsInLevelMap())
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

	//첫 웨이브 시작 웨이브는 이제 몰아내기가 아니라 시간 눈금이라 스폰은 아래 타이머가 맡음
	StartNextWave();

	//주기마다 스폰 볼륨에게 내라고 신호 첫 신호도 주기만큼 기다렸다 나감
	//레벨 시작과 동시에 눈앞에 몬스터가 튀어나오지 않게 하려는 것
	ScheduleContinuousSpawn();

	//제한 시간과 별개로 잠식도도 차오르기 시작함
	//제한 시간은 "다 못 깼다" 판정이고 잠식도는 "버티지 못했다" 판정으로 서로 독립임
	StartCorruption();
}

// 웨이브
// 30초마다 한 번씩 스폰 볼륨들에게 몬스터를 내라고 시킴
// 마지막 웨이브를 내면 bAllMonstersSpawned를 켜서 기존 클리어 판정(TryClearLevel)이 그대로 동작함

//다음 웨이브를 내보냄
void AMainGameModeBase::StartNextWave()
{
	CurrentWave++;

	//마지막 웨이브까지 다 지났으면 잡몹 웨이브는 여기서 끝이고 이제 보스 차례
	//웨이브가 끝나는 순간이 아니라 마지막 웨이브가 제 시간을 다 쓴 뒤에 멈춰야
	//6웨이브에도 몬스터가 계속 나오다가 끊김 여기서 바로 멈추면 6웨이브가 텅 빈 채로 끝남
	if (CurrentWave > WaveCount)
	{
		GetWorldTimerManager().ClearTimer(WaveTimerHandle);

		//보스는 웨이브가 다 지난 시각에 나옴 30초 x 6웨이브면 3분
		//보스가 나오면 SpawnBoss 안에서 보스 곡으로 갈리고 남은 2분 안에 잡아야 클리어됨
		if (!bBossSpawned)
		{
			bBossSpawned = SpawnBoss() != nullptr;
		}

		//보스가 나왔으면 잡몹 스폰을 멈추지 않고 클리어 판정도 걸지 않음
		//스폰을 멈추지 않는 이유 잠식도는 몬스터를 잡아야 내려가는데 보스만 남기면 보스를 잡기 전에 잠식으로 먼저 죽음
		//NotifyAllMonstersSpawned를 안 부르는 이유 이걸 부르면 잡몹이 잠깐 0마리가 된 순간 보스를 두고 클리어돼버림
		//그래서 보스가 있는 레벨의 클리어 조건은 보스 사망 하나뿐임 HandleBossDead가 맡음
		if (bBossSpawned)
		{
			return;
		}

		//여기부터는 보스를 못 낸 맵의 안전장치 예전처럼 잡몹을 다 잡으면 클리어시킴
		//이게 없으면 보스 클래스를 안 꽂아둔 맵에서 레벨이 영원히 끝나지 않음
		GetWorldTimerManager().ClearTimer(ContinuousSpawnTimerHandle);

		NotifyAllMonstersSpawned();

		return;
	}

	OnWaveChanged.Broadcast(CurrentWave, WaveCount);

	//다음 웨이브 예약 이미 돌고 있으면 다시 걸지 않아도 되지만
	//첫 웨이브는 타이머 없이 들어오므로 여기서 한 번 걸어둠
	if (!GetWorldTimerManager().IsTimerActive(WaveTimerHandle))
	{
		GetWorldTimerManager().SetTimer(WaveTimerHandle, this, &AMainGameModeBase::StartNextWave, WaveInterval, true);
	}
}

//맵에 있는 스폰 볼륨 전부에게 이번 웨이브 몬스터를 내라고 시킴
void AMainGameModeBase::RequestContinuousSpawn()
{
	//다음 신호부터 먼저 예약함 아래에서 중간에 돌아가도 스폰이 끊기지 않게
	//Endless는 시간이 갈수록 주기가 줄어들어서 신호마다 새로 계산해 걸어야 함
	ScheduleContinuousSpawn();

	//이미 꽉 찼으면 이번 신호는 건너뜀 볼륨을 여러 개 놓아도 총 수는 여기 한 곳에서 막힘
	//타이머를 멈추지 않는 이유 몇 마리 잡아서 자리가 나면 다음 신호에 바로 다시 나와야 함
	if (AliveMonsterCount >= GetMaxAliveMonsters())
	{
		return;
	}

	//볼륨을 미리 모아두지 않고 매번 찾는 이유
	//도중에 볼륨이 생기거나 사라져도 알아서 반영되고 목록을 관리할 필요가 없음
	//몇 마리를 낼지 여기서 정하지 않는 이유 좁은 방과 넓은 마당의 적정 밀도가 다름 볼륨이 스스로 정함
	for (TActorIterator<AMonsterSpawnVolume> VolumeIterator(GetWorld()); VolumeIterator; ++VolumeIterator)
	{
		VolumeIterator->SpawnTick();
	}
}

//다음 스폰 신호를 예약함
//반복 타이머를 쓰지 않는 이유 주기가 레벨마다 다르고 Endless에서는 시간이 갈수록 줄어듦
//반복 타이머는 걸 때 정한 주기를 끝까지 쓰기 때문에 Endless가 처음 주기 그대로 돌아버림
void AMainGameModeBase::ScheduleContinuousSpawn()
{
	GetWorldTimerManager().SetTimer(ContinuousSpawnTimerHandle, this, &AMainGameModeBase::RequestContinuousSpawn, GetContinuousSpawnInterval(), false);
}

//지금 레벨의 표 번호
int32 AMainGameModeBase::GetStageIndex() const
{
	const UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>();
	const int32 LevelNumber = DreamVeilGameInstance ? DreamVeilGameInstance->GetCurrentLevelNumber() : 0;

	//레벨 맵이 아니면 0이 돌아옴 로비나 테스트 맵에서는 첫 레벨 값을 씀
	return FMath::Clamp(LevelNumber - 1, 0, static_cast<int32>(UE_ARRAY_COUNT(MAX_ALIVE_MONSTERS_BY_STAGE)) - 1);
}

//동시에 살아 있을 수 있는 몬스터 수
int32 AMainGameModeBase::GetMaxAliveMonsters() const
{
	const UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>();

	if (DreamVeilGameInstance && DreamVeilGameInstance->IsInEndless())
	{
		return ENDLESS_MAX_ALIVE_MONSTERS;
	}

	return MAX_ALIVE_MONSTERS_BY_STAGE[GetStageIndex()];
}

//스폰 신호를 보내는 주기
float AMainGameModeBase::GetContinuousSpawnInterval() const
{
	const UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>();

	if (DreamVeilGameInstance && DreamVeilGameInstance->IsInEndless())
	{
		//버틴 시간만큼 촘촘해짐
		//월드 시간을 쓰는 이유 맵을 연 순간 0에서 시작하고 게임을 멈추면 같이 멈춤
		//증강을 고르는 동안 스폰이 빨라지지 않게 하려는 것 몬스터 스펙 계산도 같은 기준을 씀
		const float ElapsedMinutes = GetWorld() ? GetWorld()->GetTimeSeconds() / 60.0f : 0.0f;
		const float StartInterval = SPAWN_INTERVAL_BY_STAGE[UE_ARRAY_COUNT(SPAWN_INTERVAL_BY_STAGE) - 1];

		return FMath::Max(
			StartInterval - ENDLESS_SPAWN_INTERVAL_DECAY_PER_MINUTE * ElapsedMinutes,
			ENDLESS_MIN_SPAWN_INTERVAL);
	}

	return SPAWN_INTERVAL_BY_STAGE[GetStageIndex()];
}

//무한 모드 제한 시간도 웨이브도 없음
//몬스터는 계속 나오고 시간이 갈수록 세지며 보스가 주기적으로 끼어듦
//스펙이 세지는 계산은 UMonsterProgressionLibrary가 월드 시간을 보고 하므로 여기서는 낼 때만 정함
void AMainGameModeBase::StartEndlessMode()
{
	//앞으로 스폰될 몬스터를 세기 위해 레벨 맵과 똑같이 스폰 알림을 받음
	//살아있는 수를 세는 이유 클리어 조건은 없지만 HUD가 남은 적 수를 보여줄 수 있어야 함
	GetWorld()->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &AMainGameModeBase::HandleActorSpawned));

	for (TActorIterator<AMonsterBase> MonsterIterator(GetWorld()); MonsterIterator; ++MonsterIterator)
	{
		RegisterMonster(*MonsterIterator);
	}

	ScheduleContinuousSpawn();

	//첫 보스도 주기만큼 기다렸다 나옴 들어가자마자 보스와 마주치지 않게
	GetWorldTimerManager().SetTimer(EndlessBossTimerHandle, this, &AMainGameModeBase::SpawnEndlessBoss, FMath::Max(EndlessBossInterval, 1.0f), true);

	//잠식도는 레벨과 같은 규칙으로 돎 Endless라는 이유로 다르게 굴지 않음
	StartCorruption();
}

void AMainGameModeBase::SpawnEndlessBoss()
{
	//앞 보스가 아직 살아 있어도 또 냄 오래 버틸수록 보스가 쌓이는 것이 무한 모드의 압박
	//스펙은 몬스터가 스스로 월드 시간을 보고 걸기 때문에 나중에 나온 보스일수록 셈
	AMonsterBase* Boss = SpawnBoss();

	if (!Boss)
	{
		return;
	}

	EndlessBossCount++;

	//몇 번째 보스인지만큼 증강을 붙임 첫 보스는 하나 다섯 번째 보스는 다섯 개
	//시간 배율과 따로 두는 이유 배율은 숫자만 커지는데 증강은 굴리는 방식이 바뀌어서 체감이 다름
	//풀에서 뽑기 때문에 같은 순서의 보스라도 판마다 다른 조합이 나옴
	//뽑을 수 있는 증강은 보스 전용 목록으로 제한돼 있음 ABossMonster 생성자 참고
	UDispatchTableComponent* BossDispatchTable = Boss->FindComponentByClass<UDispatchTableComponent>();

	if (!BossDispatchTable)
	{
		return;
	}

	for (int32 AugmentIndex = 0; AugmentIndex < EndlessBossCount; AugmentIndex++)
	{
		//한 번 뽑힌 고유 증강은 풀에서 빠지고 공방체 증가만 계속 쌓임
		//다 떨어지면 false가 돌아오는데 그때는 더 붙일 것이 없다는 뜻이라 그냥 둠
		BossDispatchTable->DrawAndApplyAugment();
	}

	UE_LOG(LogTemp, Log, TEXT("[Endless] %d번째 보스 등장 증강 %d개"), EndlessBossCount, EndlessBossCount);
}

//지금 몇 번째 웨이브인지
int32 AMainGameModeBase::GetCurrentWave() const
{
	return CurrentWave;
}

//전체 웨이브 수
int32 AMainGameModeBase::GetWaveCount() const
{
	return WaveCount;
}

//보스를 냄
AMonsterBase* AMainGameModeBase::SpawnBoss()
{
	if (!BossClass)
	{
		return nullptr;
	}

	//플레이어 위치를 기준으로 앞쪽에 냄 보스 전용 스폰 지점이 생기면 그쪽으로 바꿀 것
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	if (!PlayerPawn)
	{
		return nullptr;
	}

	const FVector SpawnLocation = PlayerPawn->GetActorLocation() + PlayerPawn->GetActorForwardVector() * BossSpawnDistance;

	FActorSpawnParameters SpawnParameters;

	//바닥이나 벽에 조금 겹쳐도 일단 나오게 함 안 그러면 보스가 안 나와서 레벨이 끝나지 않음
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AMonsterBase* Boss = GetWorld()->SpawnActor<AMonsterBase>(BossClass, SpawnLocation, PlayerPawn->GetActorRotation(), SpawnParameters);

	//보스 판정은 액터 태그로 하므로 블루프린트에서 태그를 빠뜨렸어도 여기서 달아줌
	//이게 없으면 보스를 잡아도 클리어가 안 됨
	if (Boss && !Boss->ActorHasTag(BOSS_TAG))
	{
		Boss->Tags.Add(BOSS_TAG);

		//태그가 없는 채로 등록됐으니 보스 사망 구독을 여기서 걸어줌
		if (Boss->FindComponentByClass<UCombatStatsComponent>())
		{
			//실제 대기는 몬스터가 담당하고 게임모드는 전달된 처치 완료 신호만 받음
			Boss->OnDeathReported.AddUObject(this, &AMainGameModeBase::HandleBossDead);
		}
	}

	//보스를 못 냈으면 부른 쪽이 예전 클리어 방식으로 돌아가야 하므로 실패를 알림
	if (!Boss)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Level] 보스를 못 냄 BossClass와 스폰 위치 확인"));

		return nullptr;
	}

	//보스가 나왔으니 보스 곡으로 갈아 끼움
	//여기서 직접 PlayBGM을 부르지 않는 이유 나올 때와 죽을 때가 같은 판정을 쓰게 하려는 것
	UpdateBGMForBoss();

	return Boss;
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

	//보스의 처치 수도 5초 뒤 신호로 반영해야 마지막 몬스터 판정이 지연을 건너뛰지 않음
	Monster->OnDeathReported.AddUObject(this, &AMainGameModeBase::HandleMonsterDead);

	//보스는 죽는 순간 따로 클리어 처리 L4는 보스를 잡아야 Endless가 열림
	//살아있는 수에서도 빠져야 하므로 위의 HandleMonsterDead 구독은 그대로 둠
	if (IsBossMonster(Monster))
	{
		//아래 클리어 처리는 그대로 두고 보스가 보내는 5초 뒤 신호에 연결함
		Monster->OnDeathReported.AddUObject(this, &AMainGameModeBase::HandleBossDead);
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
	//살아 있는 보스가 더 없으면 원래 곡으로 돌아감
	//Endless는 보스가 쌓이므로 한 마리 죽었다고 바로 돌리면 안 됨
	UpdateBGMForBoss();

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

	//보스가 나온 레벨은 여기까지 오지 않음 bAllMonstersSpawned를 켜지 않아서 위에서 바로 돌아감
	//보스를 잡는 것이 다음 레벨 해금 조건이라 잡몹을 다 잡았다고 건너뛸 수 있으면 조건이 성립하지 않음
	//보스를 잡으면 HandleBossDead가 남은 잡몹과 상관없이 바로 클리어시킴
	//즉 여기로 오는 건 보스를 못 낸 맵뿐이고 그때는 예전처럼 다 잡으면 클리어됨
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
	GetWorldTimerManager().ClearTimer(WaveTimerHandle);
	GetWorldTimerManager().ClearTimer(ContinuousSpawnTimerHandle);

	//레벨이 끝났으면 잠식도도 멈춤 클리어 화면이 떠 있는 동안 계속 차오르면 안 됨
	GetWorldTimerManager().ClearTimer(CorruptionTimerHandle);

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

	//LV4는 재클리어도 제작진 화면을 표시함. 보상/해금/로비 저장은 종료 버튼에서 기존 흐름을 사용함.
	//Endless는 레벨 번호가 0이므로 보스를 잡아도 엔딩이 뜨지 않음.
	if (DreamVeilGameInstance->GetCurrentLevelNumber() == 4)
	{
		if (AMainPlayerController* PlayerController = Cast<AMainPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
		{
			if (PlayerController->ShowEnding())
			{
				return;
			}
		}
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

	//제한 시간 초과도 잠식도가 다 찬 것과 결과는 같음 잠식 타이머는 StopLevel이 이미 정리했음
	HandleRunFailed();
}

// 배경음

//배경음을 갈아 끼움
void AMainGameModeBase::PlayBGM(USoundBase* NewBGM)
{
	//같은 곡이면 그대로 둠 다시 틀면 처음으로 되감겨서 끊긴 것처럼 들림
	if (CurrentBGM == NewBGM)
	{
		return;
	}

	//이전 곡을 먼저 멈춤 안 멈추면 두 곡이 겹쳐서 흐름
	if (BGMAudio)
	{
		BGMAudio->Stop();
		BGMAudio->DestroyComponent();
		BGMAudio = nullptr;
	}

	CurrentBGM = NewBGM;

	if (!NewBGM)
	{
		//일부러 멈춘 것일 수도 있고(엔딩 화면) 곡을 안 꽂아둔 것일 수도 있어서 둘 다 알아보게 적음
		UE_LOG(LogTemp, Log, TEXT("[BGM] 배경음을 멈춤 일부러 멈춘 게 아니면 BP_MainGameModeBase의 BGM 칸 확인"));

		return;
	}

	//2D로 재생 배경음은 위치가 없어야 카메라가 어디를 보든 같은 크기로 들림
	//컴포넌트를 들고 있는 이유 곡을 갈아 끼울 때 멈춰야 하고 PlaySound2D는 핸들을 주지 않음
	//마지막에서 두 번째가 false인 이유 맵이 바뀔 때 이 소리도 같이 정리되게 하려는 것
	//true로 두면 월드를 비울 때 살아남는데 게임모드는 맵마다 새로 만들어져서 멈출 사람이 없어짐
	//그러면 로비 곡 위에 레벨 곡이 겹쳐 흐르고 맵을 옮길수록 계속 쌓임
	//크기는 1로 틂 실제 크기는 사운드 믹스가 배경음 클래스 전체에 걸어줌
	//여기서 크기를 곱하지 않는 이유 맵마다 새 컴포넌트가 생기는데 컴포넌트마다 값을 맞춰 주려면 빠뜨리기 쉬움
	//바로 틀지 않고 만들어만 두는 이유 재생을 시작하기 전에 UI 소리로 표시해야 하기 때문
	//게임 오버 화면과 증강 선택창이 게임을 멈추는데 UI 소리로 표시하지 않으면 배경음도 같이 멈춤
	//화면만 멈춘 게 아니라 게임이 죽은 것처럼 들려서 게임 오버 곡이 아예 안 들리는 문제가 됨
	BGMAudio = UGameplayStatics::CreateSound2D(this, NewBGM, 1.0f, 1.0f, 0.0f, nullptr, false, false);

	if (BGMAudio)
	{
		BGMAudio->bIsUISound = true;

		BGMAudio->Play();
	}

	//새 맵에도 믹스를 다시 걸어둠 같은 믹스면 아무 일도 하지 않으므로 중복 호출이 안전함
	if (UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>())
	{
		DreamVeilGameInstance->ApplyBGMVolume();
	}

	//안 들릴 때 에셋이 안 꽂힌 건지 재생이 실패한 건지 구분하려고 남김
	UE_LOG(LogTemp, Log, TEXT("[BGM] %s 재생 %s"), *NewBGM->GetName(), BGMAudio ? TEXT("성공") : TEXT("실패"));
}

//게임 오버 곡으로 갈아 끼움
void AMainGameModeBase::PlayGameOverBGM()
{
	PlayBGM(GameOverBGM);
}

//지금 맵에 맞는 기본 곡으로 되돌림
void AMainGameModeBase::PlayBaseBGM()
{
	PlayBGM(GetBaseBGM());
}

//지금 맵에 맞는 기본 배경음
USoundBase* AMainGameModeBase::GetBaseBGM() const
{
	const UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>();

	if (!DreamVeilGameInstance)
	{
		return LobbyBGM;
	}

	if (DreamVeilGameInstance->IsInEndless())
	{
		return EndlessBGM;
	}

	//레벨 맵이 아니면 로비 곡 메인 메뉴는 이 게임모드를 쓰지 않아서 여기로 오지 않음
	//테스트 맵이 여기로 오는데 싸우지 않는 곳이라 로비 곡이면 충분함
	return DreamVeilGameInstance->IsInLevelMap() ? LevelBGM : LobbyBGM;
}

//살아 있는 보스가 있으면 보스 곡 없으면 기본 곡
void AMainGameModeBase::UpdateBGMForBoss()
{
	//보스 곡을 안 넣어뒀으면 갈아 끼울 것이 없음
	if (!BossBGM)
	{
		return;
	}

	for (TActorIterator<AMonsterBase> MonsterIterator(GetWorld()); MonsterIterator; ++MonsterIterator)
	{
		AMonsterBase* Monster = *MonsterIterator;

		if (!IsValid(Monster) || !IsBossMonster(Monster))
		{
			continue;
		}

		//죽는 중인 보스는 아직 월드에 남아 있으므로 스탯으로 살아 있는지 확인함
		const UCombatStatsComponent* BossStats = Monster->FindComponentByClass<UCombatStatsComponent>();

		if (BossStats && BossStats->IsDead())
		{
			continue;
		}

		PlayBGM(BossBGM);

		return;
	}

	PlayBGM(GetBaseBGM());
}

//잠식도를 올리기 시작함
void AMainGameModeBase::StartCorruption()
{
	Corruption = 0.0f;

	//0 이하가 되면 타이머가 매 프레임 돌아서 최소값으로 막음
	GetWorldTimerManager().SetTimer(CorruptionTimerHandle, this, &AMainGameModeBase::TickCorruption, FMath::Max(CorruptionTickInterval, 0.01f), true);
}

//잠식도가 차오르는 데 걸리는 시간
float AMainGameModeBase::GetCorruptionFillTime() const
{
	const UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>();

	if (DreamVeilGameInstance && DreamVeilGameInstance->IsInEndless())
	{
		//버틴 시간만큼 빨리 참 스폰 주기와 같은 기준이라 후반에 몬스터도 잠식도도 같이 몰아침
		const float ElapsedMinutes = GetWorld() ? GetWorld()->GetTimeSeconds() / 60.0f : 0.0f;
		const float StartFillTime = CORRUPTION_FILL_TIME_BY_STAGE[UE_ARRAY_COUNT(CORRUPTION_FILL_TIME_BY_STAGE) - 1];

		return FMath::Max(
			StartFillTime - ENDLESS_CORRUPTION_FILL_DECAY_PER_MINUTE * ElapsedMinutes,
			ENDLESS_MIN_CORRUPTION_FILL_TIME);
	}

	return CORRUPTION_FILL_TIME_BY_STAGE[GetStageIndex()];
}

//주기마다 잠식도를 올림
void AMainGameModeBase::TickCorruption()
{
	//Endless는 시간이 갈수록 값이 바뀌므로 매 주기 다시 물어봄
	const float CorruptionFillTime = GetCorruptionFillTime();

	//채우는 시간이 0이면 나눌 수 없음 표에 0을 넣어 잠식도를 끄고 싶을 때를 위해 남겨둠
	if (CorruptionFillTime <= 0.0f)
	{
		return;
	}

	//한 주기에 오르는 양은 "주기 나누기 총 시간" 총 시간을 바꾸면 속도가 그대로 따라옴
	Corruption += FMath::Max(CorruptionTickInterval, 0.01f) / CorruptionFillTime;

	if (Corruption < 1.0f)
	{
		return;
	}

	Corruption = 1.0f;

	HandleCorruptionFull();
}

//잠식도를 내림
void AMainGameModeBase::ReduceCorruption(float Amount)
{
	//잠식도가 돌고 있지 않으면(로비 메인메뉴 끝난 레벨) 내릴 것도 없음
	if (!CorruptionTimerHandle.IsValid() || Amount <= 0.0f)
	{
		return;
	}

	Corruption = FMath::Max(Corruption - Amount, 0.0f);
}

float AMainGameModeBase::GetCorruption() const
{
	return Corruption;
}

float AMainGameModeBase::GetCorruptionPercent() const
{
	return Corruption * 100.0f;
}

//잠식도가 1에 닿음
void AMainGameModeBase::HandleCorruptionFull()
{
	//잠식 타이머를 먼저 멈춤 게임 오버 창이 떠 있는 동안 계속 돌면 안 됨
	GetWorldTimerManager().ClearTimer(CorruptionTimerHandle);

	//레벨이면 StopLevel이 제한 시간과 웨이브 스폰을 같이 정리함
	//Endless는 LevelTimerHandle이 없어서 false가 돌아오므로 남은 타이머를 직접 멈춤
	if (!StopLevel())
	{
		GetWorldTimerManager().ClearTimer(ContinuousSpawnTimerHandle);
		GetWorldTimerManager().ClearTimer(EndlessBossTimerHandle);
	}

	HandleRunFailed();
}

//한 판이 실패로 끝났을 때의 공통 처리
void AMainGameModeBase::HandleRunFailed()
{
	//죽은 뒤에 끝났으면 로비로 보내지 않음 게임 오버 쪽 흐름과 겹치지 않게
	if (IsPlayerDead())
	{
		return;
	}

	UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>();

	if (!DreamVeilGameInstance)
	{
		return;
	}

	bTimedOut = true;
	if (auto* PC = Cast<AMainPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		if (PC->ShowCorruptionGameOver()) return;
	}
	// Keep the old safe exit if a level has no configured Game Over widget/controller.
	UE_LOG(LogTemp, Error, TEXT("Corruption timeout: Game Over UI unavailable; returning to lobby."));
	DreamVeilGameInstance->FailCurrentLevel();
}

//보스 몬스터인지
bool AMainGameModeBase::IsBossMonster(const AActor* Actor)
{
	return Actor && Actor->ActorHasTag(BOSS_TAG);
}

float AMainGameModeBase::GetLevelTimeRemaining() const
{
	// 타이머가 없으면 0
	if (!LevelTimerHandle.IsValid())
	{
		return 0.0f;
	}
	// 현재 남은 시간
	return GetWorldTimerManager().GetTimerRemaining(LevelTimerHandle);
}
float AMainGameModeBase::GetLevelTimeLimit() const
{
	// 전체 제한 시간
	return LevelTimeLimit;
}
float AMainGameModeBase::GetLevelTimeProgress() const
{
	if (bTimedOut) return 1.0f;
	// Lobby/Endless/finished levels must not look fully corrupted just because there is no timer.
	if (!LevelTimerHandle.IsValid()) return 0.0f;
	// 잘못된 값 방지
	if (LevelTimeLimit <= 0.0f)
	{
		return 0.0f;
	}
	const float RemainingTime = GetLevelTimeRemaining();
	// 시간이 줄수록 0 → 1
	return FMath::Clamp(
		1.0f - (RemainingTime / LevelTimeLimit),
		0.0f,
		1.0f
	);
}
