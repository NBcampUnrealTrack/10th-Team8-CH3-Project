#pragma once

#include "CoreMinimal.h"
#include "MonsterBase.h"
#include "MonsterSkill.h"
#include "BossMonster.generated.h"

//보스의 페이즈가 바뀌었을 때 HUD가 체력 바 단계 표시나 연출에 씀
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnBossPhaseChanged,
	int32, NewPhase,
	int32, TotalPhases
);

//보스가 가지는 페이즈 수 체력을 이 수만큼 나눠서 단계가 올라감
//4로 둔 이유 기획에서 페이즈 1~4로 정함
//였는데 기획이 바뀌었구요 3페이즈긴한데... 일단 둠
const int32 BOSS_PHASE_COUNT = 4;

//레벨마다 하나씩 나오는 보스 Endless에서는 주기적으로 나옴
//잡몹과 다른 점은 두 가지뿐임 체력이 줄면 페이즈가 오르고 페이즈에 맞는 스킬을 스스로 씀
//스킬 자체는 UMonsterSkill이 이미 페이즈와 레벨로 걸러주므로 여기서는 언제 쓸지만 정함
//보스 판정에 쓰는 Boss 태그를 생성자에서 스스로 붙임
//레벨에 미리 놓아둔 보스도 게임모드가 낸 보스와 똑같이 클리어 조건과 드롭에 잡히게 하려는 것
UCLASS()
class DREAMVEIL_API ABossMonster : public AMonsterBase
{
	GENERATED_BODY()

public:
	ABossMonster();

	//평타를 시작하기 전에 준비된 스킬부터 시도하고, 발동할 스킬이 없을 때만 부모 평타를 실행함
	virtual bool StartAttack(AActor* Target) override;

	//페이즈가 바뀔 때마다 알림 HUD가 받을 것
	UPROPERTY(BlueprintAssignable, Category = "Monster|Boss")
	FOnBossPhaseChanged OnBossPhaseChanged;

	//지금 페이즈 1부터 시작
	UFUNCTION(BlueprintPure, Category = "Monster|Boss")
	int32 GetCurrentPhase() const;

	//전체 페이즈 수 HUD가 "2 / 4" 같은 표시에 씀
	UFUNCTION(BlueprintPure, Category = "Monster|Boss")
	int32 GetPhaseCount() const;

	virtual void OnDeath() override;

protected:
	virtual void BeginPlay() override;
	virtual void MonsterInit() override;

	TObjectPtr<UCapsuleComponent> MonsterCapsuleCollisionComponent;
	TObjectPtr<USkeletalMeshComponent> MonsterSkeletalMeshComponent;

	//일반 맵은 레벨로만 패턴을 해금함. 켜면 체력 페이즈도 사용하며 엔드리스에서는 자동으로 켜짐
	//플레이 도중 바꾸면 스킬의 진행 조건과 어긋나므로 시작 전에 BP 기본값이나 배치 인스턴스에서 설정함
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Monster|Boss")
	bool bUseHealthPhases = false;

	//페이즈가 오를 때마다 이동 속도에 곱해지는 값 페이즈 4면 세 번 곱해짐
	//1.0으로 두면 속도가 안 변하고 페이즈 연출은 스킬로만 드러남
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Boss")
	float PhaseSpeedMultiplier = 1.08f;

	//스킬을 쓸지 재는 주기 초 이 간격마다 쓸 수 있는 스킬이 있는지 확인함
	//스킬의 쿨타임은 UMonsterSkill이 따로 재므로 이 값은 반응 속도에 가까움
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Boss")
	float SkillCheckInterval = 1.0f;

	//페이즈가 오를 때마다 확인 주기에 곱해지는 값 1보다 작으면 뒤 페이즈일수록 스킬이 잦아짐
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Monster|Boss")
	float PhaseSkillIntervalMultiplier = 0.8f;

private:
	//체력이 바뀔 때마다 페이즈를 다시 계산함
	//델리게이트가 이전 값과 새 값을 주는데 페이즈는 비율로 정해서 둘 다 안 씀 비율은 컴포넌트에 물어봄
	UFUNCTION()
	void HandleHealthChanged(float OldValue, float NewValue);

	//지금 체력 비율에 맞는 페이즈로 올림 내려가지는 않음
	//회복 수단이 생겨도 페이즈가 되돌아가면 연출과 스킬이 오락가락해서 올라가기만 하게 막음
	void UpdatePhase(float HealthPercentage);

	//스킬 컴포넌트에 지금 페이즈와 레벨을 알려줌
	//레벨을 구하는 방법(Endless면 마지막 레벨로 취급)이 시작할 때와 페이즈가 오를 때 똑같아서 한 곳에 둠
	void ApplySkillProgression();

	//쓸 수 있는 스킬 중 하나를 무작위로 씀 없으면 아무 일도 안 함
	//실제로 스킬을 시작했는지 반환해 평타 진입점에서도 같은 선택 로직을 재사용함
	bool TryUseRandomSkill();

	//스킬 확인 타이머를 지금 페이즈에 맞는 간격으로 다시 검
	void RestartSkillTimer();

	//지금 페이즈 1부터 BOSS_PHASE_COUNT까지
	int32 CurrentPhase = 1;

	//스킬을 쓸지 재는 타이머
	FTimerHandle SkillCheckTimerHandle;
};
