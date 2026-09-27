#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AugmentTypes.h"
#include "AugmentPool.h"
#include "Engine/HitResult.h"
#include "DispatchTableComponent.generated.h"

class UAugmentSkillBase;
class UCombatStatsComponent;
class AThornSpikeEffect;
class UParticleSystem;

//증강을 뽑고 적용하고 스킬에게 알림을 전달하는 컴포넌트
//스탯은 들고 있지 않고 같은 액터의 UCombatStatsComponent를 찾아서 스킬이 그걸 바꿈
//플레이어 보스 일반 몬스터는 UCombatStatsComponent와 이 컴포넌트를 둘 다 붙임
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DREAMVEIL_API UDispatchTableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	//생성자
	UDispatchTableComponent();

	//같은 액터의 스탯 컴포넌트 BeginPlay에서 찾아둠 스킬이 스탯을 바꿀 때 씀
	UFUNCTION(BlueprintPure, Category = "Augment")
	UCombatStatsComponent* GetStatsComponent() const;

	// 증강

	//뽑을 수 있는 증강 목록 에디터에서 바로 수정 가능 일반 몬스터는 안 씀
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment")
	FAugmentPool AugmentPool;

	//보상 UI 선택지 AUGMENT_CHOICE_COUNT개를 겹치지 않게 뽑음 풀은 건드리지 않음 플레이어용
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool DrawAugmentChoices(TArray<EAugmentID>& OutAugmentIDs);

	//증강을 적용하고 반복 획득이 안 되면 풀에서 제거 보상 UI에서 고른 것을 넘길 것
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool ApplyAugment(EAugmentID AugmentID);

	//풀에서 하나 뽑아서 바로 적용 보스용
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool DrawAndApplyAugment();

	//풀과 상관없이 증강 효과만 실행 일반 몬스터 타입별 패시브용 BeginPlay 이후에 부를 것
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool ExecuteAugment(EAugmentID AugmentID);

	//정해진 증강 묶음을 한 번에 실행
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void ExecuteAugments(const TArray<EAugmentID>& AugmentIDs);

	//같은 증강을 여러 번 실행 엔드리스 웨이브 체공방 증가용
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void ExecuteAugmentRepeat(EAugmentID AugmentID, int32 RepeatCount);

	//이미 얻은 증강인지 여부
	UFUNCTION(BlueprintPure, Category = "Augment")
	bool HasAcquiredAugment(EAugmentID AugmentID) const;

	// UI 표시용 조회

	//증강 이름 얻어서 풀에서 빠진 증강도 조회됨
	UFUNCTION(BlueprintPure, Category = "Augment")
	static FText GetAugmentDisplayName(EAugmentID AugmentID);

	//증강 효과 설명 수치는 AugmentTypes.h 값을 그대로 씀
	UFUNCTION(BlueprintPure, Category = "Augment")
	static FText GetAugmentDescription(EAugmentID AugmentID);

	//풀에 남아 있는 증강 정보 가중치와 반복 획득 여부까지 필요할 때 씀 풀에 없으면 false
	UFUNCTION(BlueprintPure, Category = "Augment")
	bool FindAugmentData(EAugmentID AugmentID, FAugmentData& OutAugmentData) const;

	//ApplyAugment로 얻은 증강 기록 레벨을 넘길 때 저장용
	UFUNCTION(BlueprintPure, Category = "Augment")
	TArray<EAugmentID> GetAugmentHistory() const;

	//저장해둔 기록대로 증강을 다시 얻음 스탯 스킬 풀 상태가 함께 복원됨
	//BeginPlay 이후에 한 번만 부를 것 이미 얻은 증강이 있으면 중복 적용을 막으려고 실패함
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool RestoreAugments(const TArray<EAugmentID>& History);

	// 스킬 알림 AugmentDamageLibrary가 부름

	//받은 데미지로 공격자에게 돌려줄 반사 데미지 가시 갑옷이 없으면 0
	UFUNCTION(BlueprintCallable, Category = "Combat")
	float CalculateThornReflectDamage(float FinalDamage);

	//데미지를 입힌 뒤 보유 스킬 효과를 처리 흡혈
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ProcessOnDamageDealt(float FinalDamage);

	//무기가 무언가를 맞혔을 때 보유 스킬 효과를 처리 범위 공격 감속 지속 공격
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ProcessWeaponHit(const FHitResult& HitResult, float HitDamage);

	// 증강 연출
	// 증강을 가진 쪽이 자기 증강의 연출을 들고 있음 여기 한 곳만 채우면 됨
	// 맞는 쪽(몬스터)이 들고 있게 하면 몬스터 블루프린트마다 같은 에셋을 넣어야 해서 관리가 안 됨
	// 보스가 증강을 얻으면 보스 블루프린트의 이 칸이 쓰임

	//가시 갑옷 반사를 맞은 대상 발밑에서 솟는 가시 비우면 연출 없이 데미지만 들어감
	//C++ 생성자에 기본값이 들어 있어서 에디터에서 아무것도 안 해도 나옴
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment|Effect")
	TSubclassOf<AThornSpikeEffect> ThornSpikeEffectClass;

	//지속 공격으로 불붙은 대상 몸에 붙일 불꽃 불이 꺼지면 같이 사라짐
	//반복 재생되는 것을 넣을 것 한 번 터지고 끝나는 것을 넣으면 3초 타는 동안 불이 먼저 꺼짐
	//폭발과 같은 Cascade인 이유 쓰려는 에셋(P_Fire_Small 등)이 Cascade라 변환 없이 바로 넣으려는 것
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Augment|Effect")
	TObjectPtr<UParticleSystem> OnFireEffect;

	//가시 반사를 맞은 대상 발밑에 가시를 냄 반사가 실제로 들어갔을 때 AugmentDamageLibrary가 부름
	UFUNCTION(BlueprintCallable, Category = "Augment|Effect")
	void PlayThornReflectEffect(AActor* ReflectTarget);

protected:
	//생명주기 함수
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	//같은 액터의 스탯 컴포넌트
	UPROPERTY(Transient)
	TObjectPtr<UCombatStatsComponent> StatsComponent;

	//디스패치 테이블 증강 번호마다 어떤 스킬 클래스를 만들지
	UPROPERTY(VisibleAnywhere, Category = "Augment")
	TMap<EAugmentID, TSubclassOf<UAugmentSkillBase>> SkillClassTable;

	//지금까지 얻은 증강의 스킬 객체 처음 얻을 때 만들어짐
	UPROPERTY(Transient)
	TMap<EAugmentID, TObjectPtr<UAugmentSkillBase>> AcquiredSkills;

	//ApplyAugment로 얻은 증강 번호 얻은 순서대로 같은 번호가 여러 번이면 그만큼 중첩
	//레벨을 넘길 때 GameInstance가 이걸 저장했다가 새 레벨의 플레이어에게 RestoreAugments로 다시 적용함
	UPROPERTY(Transient)
	TArray<EAugmentID> AugmentHistory;

	//증강 번호와 스킬 클래스를 테이블에 등록
	void RegisterSkillClasses();

	//얻은 스킬이 있으면 돌려주고 없으면 테이블을 보고 새로 만듦 테이블에 없는 번호면 nullptr
	UAugmentSkillBase* FindOrCreateSkill(EAugmentID AugmentID);

	//얻은 스킬 목록을 배열로 복사 스킬을 부르는 도중 증강을 얻거나 컴포넌트가 정리돼 맵이 바뀌어도 안전하게 돌기 위함
	TArray<TObjectPtr<UAugmentSkillBase>> GetAcquiredSkillsSnapshot() const;

	//스탯 컴포넌트의 체력 변화를 받아 스킬에게 전달 광전사 최후의 요새
	UFUNCTION()
	void HandleCurrentHealthChanged(float OldValue, float NewValue);
};
