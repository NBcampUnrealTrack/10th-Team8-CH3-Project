#include "MonsterProgressionLibrary.h"

#include "DreamVeilGameInstance.h"
#include "EliteMonster.h"
#include "MainGameModeBase.h"
#include "MainPlayerCharacter.h"
#include "MonsterBase.h"
#include "Engine/World.h"

EMonsterGrade UMonsterProgressionLibrary::GetMonsterGrade(const AActor* Monster)
{
	//몬스터가 아니면 등급을 따질 것이 없음 잡몹으로 취급해서 호출한 쪽이 분기를 또 쓰지 않게 함
	if (!Cast<AMonsterBase>(Monster))
	{
		return EMonsterGrade::Normal;
	}

	//보스 판정을 게임모드에 맡기는 이유 레벨 클리어 조건이 쓰는 기준과 반드시 같아야 함
	//여기서 따로 판단하면 보스를 잡았는데 클리어가 안 되거나 그 반대가 생김
	if (AMainGameModeBase::IsBossMonster(Monster))
	{
		return EMonsterGrade::Boss;
	}

	if (Monster->IsA<AEliteMonster>())
	{
		return EMonsterGrade::Elite;
	}

	return EMonsterGrade::Normal;
}

float UMonsterProgressionLibrary::GetDifficultyStatScale(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UDreamVeilGameInstance* DreamVeilGameInstance = World ? World->GetGameInstance<UDreamVeilGameInstance>() : nullptr;

	//게임 인스턴스가 없는 상황(에디터 프리뷰 등)에서는 배율을 걸지 않음
	if (!DreamVeilGameInstance)
	{
		return 1.0f;
	}

	const int32 DifficultyIndex = FMath::Clamp(
		static_cast<int32>(DreamVeilGameInstance->GetDifficulty()),
		0,
		static_cast<int32>(UE_ARRAY_COUNT(MONSTER_STAT_SCALE_BY_DIFFICULTY)) - 1);

	return MONSTER_STAT_SCALE_BY_DIFFICULTY[DifficultyIndex];
}

float UMonsterProgressionLibrary::GetStageStatScale(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UDreamVeilGameInstance* DreamVeilGameInstance = World ? World->GetGameInstance<UDreamVeilGameInstance>() : nullptr;

	if (!DreamVeilGameInstance)
	{
		return 1.0f;
	}

	//Endless는 정해진 배율이 없고 버틴 시간이 곧 난이도
	//월드 시간을 쓰는 이유 맵을 연 순간 0에서 시작하므로 따로 시작 시각을 들고 있지 않아도 됨
	//게임을 멈추면 월드 시간도 같이 멈춰서 증강을 고르는 동안 몬스터가 세지지 않음
	if (DreamVeilGameInstance->IsInEndless())
	{
		const float ElapsedMinutes = World->GetTimeSeconds() / 60.0f;

		return 1.0f + ENDLESS_STAT_SCALE_PER_MINUTE * ElapsedMinutes;
	}

	const int32 LevelNumber = DreamVeilGameInstance->GetCurrentLevelNumber();

	//레벨 맵이 아니면 0이 돌아옴 로비나 테스트 맵에서 몬스터를 내도 배율이 안 걸리게 함
	if (LevelNumber <= 0)
	{
		return 1.0f;
	}

	const int32 StageIndex = FMath::Clamp(
		LevelNumber - 1,
		0,
		static_cast<int32>(UE_ARRAY_COUNT(MONSTER_STAT_SCALE_BY_STAGE)) - 1);

	return MONSTER_STAT_SCALE_BY_STAGE[StageIndex];
}

float UMonsterProgressionLibrary::GetGradeStatScale(EMonsterGrade Grade)
{
	const int32 GradeIndex = FMath::Clamp(
		static_cast<int32>(Grade),
		0,
		static_cast<int32>(UE_ARRAY_COUNT(MONSTER_STAT_SCALE_BY_GRADE)) - 1);

	return MONSTER_STAT_SCALE_BY_GRADE[GradeIndex];
}

float UMonsterProgressionLibrary::GetMonsterStatScale(const UObject* WorldContextObject, EMonsterGrade Grade)
{
	return GetDifficultyStatScale(WorldContextObject)
		* GetStageStatScale(WorldContextObject)
		* GetGradeStatScale(Grade);
}

float UMonsterProgressionLibrary::GetExperienceReward(const UObject* WorldContextObject, EMonsterGrade Grade)
{
	const int32 GradeIndex = FMath::Clamp(
		static_cast<int32>(Grade),
		0,
		static_cast<int32>(UE_ARRAY_COUNT(MONSTER_EXPERIENCE_BY_GRADE)) - 1);

	const float BaseExperience = MONSTER_EXPERIENCE_BY_GRADE[GradeIndex];

	//뒤 레벨 몬스터가 더 주긴 하되 스펙만큼 주지는 않음
	//그대로 곱하면 L4 보스 한 마리로 몇 레벨이 올라서 증강 선택이 쏟아짐
	//제곱근을 쓰면 L4에서도 L1의 1.5배쯤이라 레벨을 계속 돌 이유는 남고 폭주하지는 않음
	const float StageBonus = FMath::Sqrt(GetStageStatScale(WorldContextObject));

	return BaseExperience * StageBonus;
}

//잡은 만큼 잠식도를 내림
void UMonsterProgressionLibrary::GrantKillCorruptionRelief(AActor* DeadMonster)
{
    //몬스터를 잡았을 때만 상자나 오브젝트를 부순 것은 해당 없음
    if (!Cast<AMonsterBase>(DeadMonster))
    {
        return;
    }

    //잠식도를 들고 있는 쪽이 게임모드라 여기가 없으면 내릴 것도 없음
    AMainGameModeBase* GameMode = DeadMonster->GetWorld() ? DeadMonster->GetWorld()->GetAuthGameMode<AMainGameModeBase>() : nullptr;

    if (!GameMode)
    {
        return;
    }

    const int32 GradeIndex = FMath::Clamp(
        static_cast<int32>(GetMonsterGrade(DeadMonster)),
        0,
        static_cast<int32>(UE_ARRAY_COUNT(MONSTER_CORRUPTION_RELIEF_BY_GRADE)) - 1);

    GameMode->ReduceCorruption(MONSTER_CORRUPTION_RELIEF_BY_GRADE[GradeIndex]);
}

void UMonsterProgressionLibrary::GrantKillExperience(AActor* Killer, AActor* DeadMonster)
{
	//몬스터를 잡았을 때만 경험치가 나감 상자나 오브젝트를 부순 것은 해당 없음
	if (!Cast<AMonsterBase>(DeadMonster))
	{
		return;
	}

	//플레이어가 잡았을 때만 몬스터끼리 죽이는 경우는 지금 없지만 들어와도 안전하게
	AMainPlayerCharacter* Player = Cast<AMainPlayerCharacter>(Killer);

	if (!Player)
	{
		return;
	}

	//죽은 뒤에 들어온 경험치는 AddExperience가 알아서 무시함 여기서 또 확인하지 않음
	Player->AddExperience(GetExperienceReward(Player, GetMonsterGrade(DeadMonster)));
}
