#include "BossMonster.h"

#include "CombatStatsComponent.h"
#include "DreamVeilGameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Components/ShapeComponent.h"
#include "Components/CapsuleComponent.h"
#include "MonsterAIController.h"

ABossMonster::ABossMonster()
{
	//보스 태그를 스스로 붙임
	//게임모드가 낸 보스든 레벨에 미리 놓아둔 보스든 똑같이 클리어 조건 드롭 등급에 잡히게 하려는 것
	//태그를 잊고 안 붙이면 잡아도 레벨이 안 끝나고 보스 파츠도 안 떨어짐
	Tags.Add(TEXT("Boss"));

	//보스 체력은 판마다 달라지면 안 되므로 위아래를 같은 값으로 둠
	//부모는 이 둘 사이에서 무작위로 뽑는데 보스가 어떤 판은 약하고 어떤 판은 센 것은 말이 안 됨
	//등급 배율 6이 곱해져서 L1 쉬움 실효 3600 플레이어가 페이즈 4개를 도는 데 약 40초 걸림
	MinHealthRadius = 600.0f;
	MaxHealthRadius = 600.0f;

	//잡몹과 같은 값 등급 배율 6의 제곱근이 곱해져서 L1 쉬움에서 약 24가 됨
	MonsterDamage = 10.0f;

	MonsterCapsuleCollisionComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule Collision"));
	MonsterCollisionComponent = MonsterCapsuleCollisionComponent;
	MonsterCollisionComponent->SetupAttachment(RootComponent);

	AIControllerClass = AMonsterAIController::StaticClass();

	AttackType = EMonsterAttackType::Ranged;
	//보스 메시에는 호환되는 공격 몽타주가 없으므로 타이머로 평타를 발사함
	//Stickman 몽타주 재생 성공 여부에 평타가 막히지 않도록 보스만 애니메이션 경로를 끔
	bUseAttackMontage = false;

	// SkeletalMeshComponent만의 고유한 기능을 쓸 수도 있으니 이렇게 두 변수로 나눕니다. 둘 다 가리키는 컴포넌트는 동일
	MonsterSkeletalMeshComponent = GetMesh();
	MonsterMeshComponent->SetupAttachment(RootComponent);

	MonsterCombatStats->SetMaxHealth(MaxHealth);

	MonsterWalkSpeed = 800.0f;
}

bool ABossMonster::StartAttack(AActor* Target)
{
	//잘못된 타깃이나 행동 불가 상태에서는 스킬 선택과 평타 모두 시작하지 않음
	if (!IsValid(Target) || Target == this || IsRagdoll()
		|| (IsValid(MonsterCombatStats) && MonsterCombatStats->IsDead()))
	{
		return false;
	}

	//주기 타이머를 기다리면 BT의 다음 평타가 먼저 시작될 수 있으므로 여기서 스킬을 먼저 선택함
	//스킬이 시작되면 평타 태스크는 실패로 끝내고, 스킬이 관리하는 AI 정지와 재개 흐름을 따름
	if (TryUseRandomSkill())
	{
		return false;
	}

	return Super::StartAttack(Target);
}

void ABossMonster::BeginPlay()
{
	Super::BeginPlay();

	//체력이 바뀔 때마다 페이즈를 다시 계산함
	//Tick으로 매 프레임 재지 않는 이유 체력은 맞았을 때만 바뀌므로 그때만 보면 충분함
	//일반 맵의 기본 동작에서는 체력 변화로 패턴이나 이동 속도가 바뀌지 않음
	if (bUseHealthPhases && IsValid(MonsterCombatStats))
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
	//체력 페이즈를 사용하는 보스만 기존 단계 상승과 속도 증가를 적용함
	if (!bUseHealthPhases) return;

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
		//엔드리스는 마지막 맵의 패턴 후보를 사용하되 체력이 줄어들 때 차례로 해금함
		bUseHealthPhases = true;
	}

	//레벨을 같이 넘기는 이유 같은 보스라도 L1에서는 기본 패턴만 L4에서는 전부 쓰게 하려는 것
	//체력 페이즈가 꺼져 있으면 RequiredPhase는 무시하고 RequiredLevel만 해금 조건으로 사용함
	MonsterSkill->SetProgression(CurrentPhase, FMath::Max(LevelNumber, 1), bUseHealthPhases);
}

void ABossMonster::RestartSkillTimer()
{
	//페이즈가 오른 만큼 간격을 줄임 2페이즈면 한 번 3페이즈면 두 번 곱해짐
	const float PhaseInterval = SkillCheckInterval * FMath::Pow(PhaseSkillIntervalMultiplier, static_cast<float>(CurrentPhase - 1));

	//0 이하가 되면 타이머가 매 프레임 돌아서 최소값으로 막음
	const float SafeInterval = FMath::Max(PhaseInterval, 0.1f);

	//추격 중에도 스킬을 확인하는 기존 타이머는 유지하며, 반환값은 평타 진입점에서만 사용함
	GetWorldTimerManager().SetTimer(SkillCheckTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		TryUseRandomSkill();
	}), SafeInterval, true);
}

bool ABossMonster::TryUseRandomSkill()
{
	if (!IsValid(MonsterSkill))
	{
		return false;
	}

	//이미 스킬을 쓰는 중이거나 평타를 치는 중이면 끼어들지 않음
	if (MonsterSkill->IsUsingSkill() || IsAttacking() || IsRagdoll())
	{
		return false;
	}

	//죽었으면 더 쓰지 않음
	if (IsValid(MonsterCombatStats) && MonsterCombatStats->IsDead())
	{
		return false;
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
		return false;
	}

	//쓸 수 있는 것 중 무작위로 하나 패턴이 순서대로 나오면 외워져서 재미가 없음
	//사용 상태 설정뿐 아니라 경고와 실제 패턴 실행까지 연결한다.
	//쿨타임이 끝났어도 거리 조건 때문에 실패할 수 있으므로 남은 후보도 무작위 순서로 시도함
	//한 스킬이 실패했다는 이유만으로 다른 사용 가능한 스킬보다 평타가 먼저 나가지 않게 함
	while (ReadySkills.Num() > 0)
	{
		const int32 SkillIndex = FMath::RandRange(0, ReadySkills.Num() - 1);
		if (MonsterSkill->TryUseSkill(ReadySkills[SkillIndex]))
		{
			return true;
		}
		ReadySkills.RemoveAtSwap(SkillIndex);
	}

	return false;
}

void ABossMonster::OnDeath()
{
	//죽고 나서도 타이머가 돌면 이미 사라진 보스가 스킬을 쓰려고 함
	GetWorldTimerManager().ClearTimer(SkillCheckTimerHandle);

	Super::OnDeath();
}
