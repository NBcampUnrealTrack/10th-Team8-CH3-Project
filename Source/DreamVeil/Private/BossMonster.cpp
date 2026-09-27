#include "BossMonster.h"

#include "CombatStatsComponent.h"
#include "DreamVeilGameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

ABossMonster::ABossMonster()
{
	//보스 태그를 스스로 붙임
	//게임모드가 낸 보스든 레벨에 미리 놓아둔 보스든 똑같이 클리어 조건 드롭 등급에 잡히게 하려는 것
	//태그를 잊고 안 붙이면 잡아도 레벨이 안 끝나고 보스 파츠도 안 떨어짐
	Tags.Add(TEXT("Boss"));
}

void ABossMonster::BeginPlay()
{
	Super::BeginPlay();

	//체력이 바뀔 때마다 페이즈를 다시 계산함
	//Tick으로 매 프레임 재지 않는 이유 체력은 맞았을 때만 바뀌므로 그때만 보면 충분함
	if (IsValid(MonsterCombatStats))
	{
		MonsterCombatStats->OnCurrentHealthChanged.AddDynamic(this, &ABossMonster::HandleHealthChanged);
	}

	RestartSkillTimer();
}

void ABossMonster::MonsterInit()
{
	Super::MonsterInit();

	//시작은 항상 1페이즈
	CurrentPhase = 1;

	//스킬마다 RequiredPhase RequiredLevel이 있어서 이 값이 낮으면 뒤 스킬이 잠겨 있음
	ApplySkillProgression();
}

int32 ABossMonster::GetCurrentPhase() const
{
	return CurrentPhase;
}

int32 ABossMonster::GetPhaseCount() const
{
	return BOSS_PHASE_COUNT;
}

void ABossMonster::HandleHealthChanged(float OldValue, float NewValue)
{
	if (!IsValid(MonsterCombatStats))
	{
		return;
	}

	UpdatePhase(MonsterCombatStats->GetHealthPercentage());
}

void ABossMonster::UpdatePhase(float HealthPercentage)
{
	//체력을 페이즈 수만큼 나눠서 지금 몇 번째 칸인지 구함
	//페이즈가 4면 100~75%가 1페이즈 75~50%가 2페이즈 식으로 나뉨
	//1에서 빼는 이유 체력이 줄수록 페이즈가 올라가야 함
	const float DepletedRatio = 1.0f - FMath::Clamp(HealthPercentage, 0.0f, 1.0f);

	//체력이 정확히 0이면 칸을 벗어나므로 마지막 페이즈로 묶음
	const int32 TargetPhase = FMath::Clamp(
		FMath::FloorToInt(DepletedRatio * BOSS_PHASE_COUNT) + 1,
		1,
		BOSS_PHASE_COUNT);

	//회복해서 체력이 올라가도 페이즈는 안 내려감
	//내려가게 두면 연출과 쓰던 스킬이 오락가락해서 플레이어가 지금 몇 페이즈인지 알 수 없음
	if (TargetPhase <= CurrentPhase)
	{
		return;
	}

	//한 방에 여러 칸이 깎여도 한 단계씩 올림
	//건너뛰면 중간 페이즈에서만 나오는 연출과 속도 증가가 통째로 빠짐
	while (CurrentPhase < TargetPhase)
	{
		CurrentPhase++;

		//페이즈가 오를 때마다 조금씩 빨라짐
		MonsterWalkSpeed *= PhaseSpeedMultiplier;
		GetCharacterMovement()->MaxWalkSpeed = MonsterWalkSpeed;
	}

	//새 페이즈에서 열리는 스킬이 있으면 여기서 풀림
	ApplySkillProgression();

	//페이즈가 오르면 스킬이 더 잦아지므로 타이머 간격도 다시 계산함
	RestartSkillTimer();

	OnBossPhaseChanged.Broadcast(CurrentPhase, BOSS_PHASE_COUNT);
}

void ABossMonster::ApplySkillProgression()
{
	if (!IsValid(MonsterSkill))
	{
		return;
	}

	const UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>();

	int32 LevelNumber = DreamVeilGameInstance ? DreamVeilGameInstance->GetCurrentLevelNumber() : 1;

	//Endless는 레벨 번호가 없으므로 마지막 레벨로 취급함 L4를 깨야 열리는 곳이라 그보다 낮을 이유가 없음
	if (DreamVeilGameInstance && DreamVeilGameInstance->IsInEndless())
	{
		LevelNumber = DreamVeilGameInstance->GetLevelCount();
	}

	//레벨을 같이 넘기는 이유 같은 보스라도 L1에서는 기본 패턴만 L4에서는 전부 쓰게 하려는 것
	MonsterSkill->SetProgression(CurrentPhase, FMath::Max(LevelNumber, 1));
}

void ABossMonster::RestartSkillTimer()
{
	//페이즈가 오른 만큼 간격을 줄임 2페이즈면 한 번 3페이즈면 두 번 곱해짐
	const float PhaseInterval = SkillCheckInterval * FMath::Pow(PhaseSkillIntervalMultiplier, static_cast<float>(CurrentPhase - 1));

	//0 이하가 되면 타이머가 매 프레임 돌아서 최소값으로 막음
	const float SafeInterval = FMath::Max(PhaseInterval, 0.1f);

	GetWorldTimerManager().SetTimer(SkillCheckTimerHandle, this, &ABossMonster::TryUseRandomSkill, SafeInterval, true);
}

void ABossMonster::TryUseRandomSkill()
{
	if (!IsValid(MonsterSkill))
	{
		return;
	}

	//이미 스킬을 쓰는 중이거나 평타를 치는 중이면 끼어들지 않음
	if (MonsterSkill->IsUsingSkill() || IsAttacking())
	{
		return;
	}

	//죽었으면 더 쓰지 않음
	if (IsValid(MonsterCombatStats) && MonsterCombatStats->IsDead())
	{
		return;
	}

	//지금 페이즈와 레벨에서 쓸 수 있고 쿨타임도 끝난 스킬만 모음
	//CanUseSkill이 페이즈 레벨 쿨타임을 다 봐주므로 여기서 다시 따지지 않음
	TArray<EMonsterSkillType> ReadySkills;

	for (const FMonsterSkillSettings& SkillSettings : MonsterSkill->Skills)
	{
		if (MonsterSkill->CanUseSkill(SkillSettings.Skill))
		{
			ReadySkills.AddUnique(SkillSettings.Skill);
		}
	}

	if (ReadySkills.Num() == 0)
	{
		return;
	}

	//쓸 수 있는 것 중 무작위로 하나 패턴이 순서대로 나오면 외워져서 재미가 없음
	MonsterSkill->BeginSkill(ReadySkills[FMath::RandRange(0, ReadySkills.Num() - 1)]);
}

void ABossMonster::OnDeath()
{
	//죽고 나서도 타이머가 돌면 이미 사라진 보스가 스킬을 쓰려고 함
	GetWorldTimerManager().ClearTimer(SkillCheckTimerHandle);

	Super::OnDeath();
}
