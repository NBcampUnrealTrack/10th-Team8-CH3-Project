// Fill out your copyright notice in the Description page of Project Settings.


#include "MonsterSpawnVolume.h"
#include "Kismet/GameplayStatics.h"

#include "MainPlayerCharacter.h"
#include "MonsterBase.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DreamVeilGameInstance.h"
#include "UObject/ConstructorHelpers.h"

//낼 자리를 몇 번까지 뽑아볼지
//튜닝할 값이 아니라 "한 번 실패했다고 포기하지 말자" 정도의 수라서 프로퍼티로 빼지 않음
const int32 SPAWN_LOCATION_TRY_COUNT = 5;

//경로로 몬스터 블루프린트를 찾아 목록에 넣음 못 찾으면 아무것도 넣지 않음
//생성자 안에서만 부를 것 ConstructorHelpers는 생성자 밖에서는 쓰지 못함
//같은 다섯 줄을 복사해 붙이지 않으려고 뺌
void AddMonsterClass(TArray<TSubclassOf<AMonsterBase>>& OutMonsters, const TCHAR* BlueprintPath)
{
	ConstructorHelpers::FClassFinder<AMonsterBase> MonsterFinder(BlueprintPath);

	if (MonsterFinder.Succeeded())
	{
		OutMonsters.Add(MonsterFinder.Class);
	}
}

AMonsterSpawnVolume::AMonsterSpawnVolume()
{
	MaxDifficultyLevel = 30;
	EliteMonsterMinRate = 5.0f;
	EliteMonsterMaxRate = 50.0f;
	DifficultyCurve = 1.0f;

	//레벨에 끌어다 놓기만 하면 바로 몬스터가 나오게 기본 목록을 코드가 채워둠
	//목록을 비워두면 볼륨을 놓아도 아무 일이 없는데 그게 배치한 사람 눈에는 고장난 것처럼 보임
	//디테일 패널에서 목록을 바꾸면 그쪽이 이김 볼륨마다 다른 몬스터를 내고 싶을 때 그렇게 쓸 것
	AddMonsterClass(BaseMonsters, TEXT("/Game/Blueprint/Monsters/BP_MeleeMonster"));
	AddMonsterClass(BaseMonsters, TEXT("/Game/Blueprint/Monsters/BP_RangedMonster"));

	//엘리트는 넣는 순서가 곧 해금 순서임 L1은 1번만 L2는 1 2번 L3부터 셋 다 나옴
	//BP_EliteMonster(번호 없는 것)를 넣지 않는 이유 쓰지 않는 블루프린트라 목록에 있으면 순서만 밀림
	AddMonsterClass(EliteMonsters, TEXT("/Game/Blueprint/Monsters/BP_EliteMonster1"));
	AddMonsterClass(EliteMonsters, TEXT("/Game/Blueprint/Monsters/BP_EliteMonster2"));
	AddMonsterClass(EliteMonsters, TEXT("/Game/Blueprint/Monsters/BP_EliteMonster3"));
}

void AMonsterSpawnVolume::ExecuteSpawnActor()
{
	FVector SpawnLocation;

	//땅 위 설 수 있는 자리를 못 찾으면 이번 한 마리는 건너뜀
	//공중이나 벽 속에 내면 몬스터가 끼거나 떨어져서 플레이어에게 오지 못함
	if (!FindSpawnLocation(SpawnLocation))
	{
		return;
	}

	const float EliteSpawnRate = GetEliteRate();

	//이 레벨에서 나올 수 있는 엘리트만 추림 L1에서 3번 엘리트가 튀어나오지 않게
	const TArray<TSubclassOf<AMonsterBase>> AllowedElites = GetEliteMonstersForCurrentLevel();

	//엘리트에 당첨됐는데 낼 엘리트가 없으면 잡몹으로 내려감
	//여기서 그냥 돌아가면 엘리트를 안 채워둔 볼륨이 아무것도 안 내서 웨이브가 통째로 비어버림
	if (FMath::FRandRange(0.0f, 100.0f) <= EliteSpawnRate && AllowedElites.Num() > 0)
	{
		SpawnOneMonster(AllowedElites, SpawnLocation);
		return;
	}

	SpawnOneMonster(BaseMonsters, SpawnLocation);
}

//이 레벨에서 나올 수 있는 엘리트만 추려서 돌려줌
TArray<TSubclassOf<AMonsterBase>> AMonsterSpawnVolume::GetEliteMonstersForCurrentLevel() const
{
	const UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>();

	//Endless는 끝까지 간 뒤에 열리는 곳이라 전부 나옴
	if (!DreamVeilGameInstance || DreamVeilGameInstance->IsInEndless())
	{
		return EliteMonsters;
	}

	const int32 LevelNumber = DreamVeilGameInstance->GetCurrentLevelNumber();

	//레벨 맵이 아니면 0이 돌아옴 로비나 테스트 맵에서는 막을 이유가 없어서 전부 씀
	if (LevelNumber <= 0)
	{
		return EliteMonsters;
	}

	//레벨 번호가 곧 쓸 수 있는 개수 L4처럼 목록보다 레벨이 높으면 목록 전체가 됨
	const int32 AllowedCount = FMath::Min(LevelNumber, EliteMonsters.Num());

	return TArray<TSubclassOf<AMonsterBase>>(EliteMonsters.GetData(), AllowedCount);
}

//낼 자리를 찾음
bool AMonsterSpawnVolume::FindSpawnLocation(FVector& OutLocation)
{
	const AMainPlayerCharacter* Player = GetPlayerCharacter();

	for (int32 TryCount = 0; TryCount < SPAWN_LOCATION_TRY_COUNT; TryCount++)
	{
		FVector Candidate;

		//내비메시 위가 아니면 다시 뽑음 볼륨이 벽에 걸쳐 있으면 절반은 여기서 걸림
		if (!TryGetRandomNavLocation(Candidate))
		{
			continue;
		}

		//플레이어 몸 안에서 솟아오르는 것만 막음 문 앞에 볼륨을 놓았을 때 생김
		if (Player && MinPlayerDistance > 0.0f
			&& FVector::Dist(Candidate, Player->GetActorLocation()) < MinPlayerDistance)
		{
			continue;
		}

		OutLocation = Candidate;

		return true;
	}

	return false;
}

AMonsterBase* AMonsterSpawnVolume::SpawnOneMonster(const TArray<TSubclassOf<AMonsterBase>>& MonsterClasses, const FVector& SpawnLocation)
{
	//낼 종류를 안 채워뒀으면 낼 것이 없음
	if (MonsterClasses.Num() == 0)
	{
		return nullptr;
	}

	//비어 있는 칸을 뽑을 수 있으므로 채워진 것만 모아서 고름
	//블루프린트에서 배열 칸만 늘리고 클래스를 안 넣어두는 일이 흔함
	TArray<TSubclassOf<AMonsterBase>> ValidClasses;

	for (const TSubclassOf<AMonsterBase>& MonsterClass : MonsterClasses)
	{
		if (MonsterClass)
		{
			ValidClasses.Add(MonsterClass);
		}
	}

	if (ValidClasses.Num() == 0)
	{
		return nullptr;
	}

	const TSubclassOf<AMonsterBase> ChosenClass = ValidClasses[FMath::RandRange(0, ValidClasses.Num() - 1)];

	//캡슐 절반 높이만큼 띄워서 냄 바닥에 파묻힌 채로 나오면 이동 컴포넌트가 밀어내느라 튕김
	FVector AdjustedLocation = SpawnLocation;

	if (const ACharacter* MonsterDefault = ChosenClass->GetDefaultObject<ACharacter>())
	{
		if (const UCapsuleComponent* DefaultCapsule = MonsterDefault->GetCapsuleComponent())
		{
			AdjustedLocation.Z += DefaultCapsule->GetScaledCapsuleHalfHeight();
		}
	}

	FActorSpawnParameters SpawnParameters;

	//좁은 곳이라 겹쳐도 일단 냄 안 그러면 몬스터가 몰린 웨이브에서 스폰이 통째로 실패함
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	//볼륨이 보는 쪽을 보고 나옴 문 앞에 놓고 화살표를 방 안쪽으로 돌려두면 문에서 걸어나온 것처럼 보임
	//플레이어 쪽을 보게 하지 않는 이유 플레이어가 문 옆에 서 있으면 문을 등지지 않고 옆을 보고 나와서
	//문에서 나왔다는 느낌이 깨짐 어차피 나온 뒤에는 AI가 바로 플레이어 쪽으로 돌림
	//Pitch Roll을 0으로 두는 이유 볼륨을 기울여 놓아도 몬스터가 누운 채로 나오지 않게
	FRotator SpawnRotation = GetActorForwardVector().Rotation();

	SpawnRotation.Pitch = 0.0f;
	SpawnRotation.Roll = 0.0f;

	//낸 몬스터를 등록하거나 스펙을 걸어주는 일은 하지 않음
	//게임모드가 액터 스폰을 지켜보고 있다가 알아서 등록하고 스펙은 몬스터가 스스로 MonsterInit에서 검
	return GetWorld()->SpawnActor<AMonsterBase>(ChosenClass, AdjustedLocation, SpawnRotation, SpawnParameters);
}

void AMonsterSpawnVolume::BeginPlay()
{
	Super::BeginPlay();

	PlayerPawn = Cast<AMainPlayerCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));

	MaxDifficultyLevel = FMath::Max(MaxDifficultyLevel, 2);

	EliteMonsterMinRate = FMath::Clamp(
		EliteMonsterMinRate,
		0.0f,
		99.0f
	);

	EliteMonsterMaxRate = FMath::Clamp(
		EliteMonsterMaxRate,
		EliteMonsterMinRate + 1.0f,
		100.0f
	);

	DifficultyCurve = FMath::Clamp(
		DifficultyCurve,
		0.1f,
		5.0f
	);
}

//플레이어를 찾아서 들고 있음
AMainPlayerCharacter* AMonsterSpawnVolume::GetPlayerCharacter()
{
	if (!PlayerPawn)
	{
		PlayerPawn = Cast<AMainPlayerCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0));
	}

	return PlayerPawn;
}

float AMonsterSpawnVolume::GetEliteRate()
{
	const AMainPlayerCharacter* Player = GetPlayerCharacter();

	//플레이어를 아직 못 찾았으면 최소 확률로 둠
	//0을 돌려주면 엘리트가 한 마리도 안 나오는데 그게 기획인지 사고인지 구분이 안 됨
	if (!Player)
	{
		return EliteMonsterMinRate;
	}

	int32 PlayerLevel = Player->GetPlayerLevel();

	float LevelAlpha = FMath::Clamp(
		static_cast<float>(PlayerLevel - 1) / (MaxDifficultyLevel - 1),
		0.0f,
		1.0f
	);
	
	// 게임 난이도가 이 경사를 눕히거나 세움. 쉬움이면 엘리트가 늦게 나오고 어려움이면 빨리 나옴
	// 최소 최대 스폰율은 그대로 둬서 위에 잡아둔 값의 뜻이 바뀌지 않음
	float CurveExponent = DifficultyCurve;

	if (const UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>())
	{
		CurveExponent *= DreamVeilGameInstance->GetSpawnCurveScale();
	}

	LevelAlpha = FMath::Pow(LevelAlpha, CurveExponent);

	return FMath::Lerp(EliteMonsterMinRate, EliteMonsterMaxRate, LevelAlpha);
}

// 웨이브 스폰
// 게임모드가 "이번 웨이브에 몇 마리 내라"고 시키면 여기서 간격을 두고 내보냄
// 실제로 한 마리를 만드는 일은 ExecuteSpawnActor가 하므로 그쪽만 고치면 스폰 방식이 바뀜

//게임모드가 주기마다 부르는 스폰 신호
//한 마리씩 내는 절차는 웨이브와 같으므로 SpawnWave를 그대로 씀
//간격을 주기의 절반으로 나눈 이유 다음 신호가 오기 전에 이번 몫을 다 내보내야 밀리지 않음
void AMonsterSpawnVolume::SpawnTick()
{
	if (MonstersPerSpawnTick <= 0)
	{
		return;
	}

	SpawnWave(MonstersPerSpawnTick, SpawnTickGap);
}

//웨이브 시작 남은 수를 더하고 타이머를 깨움
void AMonsterSpawnVolume::SpawnWave(int32 MonsterCount, float SpawnInterval)
{
	if (MonsterCount <= 0)
	{
		return;
	}

	//앞 웨이브가 아직 다 안 나왔으면 남은 수에 더함 타이머가 이어서 마저 내보냄
	PendingSpawnCount += MonsterCount;

	//이미 돌고 있으면 그대로 두고 남은 수만 늘어남 타이머를 다시 걸면 간격이 흐트러짐
	if (GetWorldTimerManager().IsTimerActive(WaveSpawnTimerHandle))
	{
		return;
	}

	//0 이하가 들어오면 타이머가 매 프레임 돌아서 최소값으로 막음
	const float SafeInterval = FMath::Max(SpawnInterval, 0.05f);

	//첫 마리도 간격만큼 기다렸다 나옴 웨이브 시작과 동시에 눈앞에 튀어나오지 않게
	GetWorldTimerManager().SetTimer(WaveSpawnTimerHandle, this, &AMonsterSpawnVolume::SpawnOneFromWave, SafeInterval, true);
}

//타이머가 돌 때마다 한 마리 내보냄
void AMonsterSpawnVolume::SpawnOneFromWave()
{
	if (PendingSpawnCount <= 0)
	{
		GetWorldTimerManager().ClearTimer(WaveSpawnTimerHandle);
		return;
	}

	PendingSpawnCount--;

	ExecuteSpawnActor();

	//마지막 한 마리를 냈으면 더 돌 이유가 없음
	if (PendingSpawnCount <= 0)
	{
		GetWorldTimerManager().ClearTimer(WaveSpawnTimerHandle);
	}
}
