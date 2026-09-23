#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MainPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UCombatStatsComponent;
class UDispatchTableComponent;
class UInventoryComponent;
class UWeaponBase;
class UAnimMontage;
class UAnimInstance;
class UAudioComponent;
class USoundBase;
enum class EWeaponSlot : uint8;
enum class EAugmentID : uint8;

struct FInputActionValue;

//경험치가 바뀌었을 때 현재 경험치와 다음 레벨까지 필요한 경험치
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnPlayerExperienceChanged,
	float, CurrentExperience,
	float, RequiredExperience
);

//레벨이 올랐을 때 새 레벨 한 번에 여러 레벨이 오르면 오른 레벨마다 한 번씩
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnPlayerLevelUp,
	int32, NewLevel
);

//들고 있는 무기가 바뀌었을 때 새 무기 슬롯
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnWeaponChanged,
	EWeaponSlot, NewSlot
);

//플레이어가 죽었을 때 한 번
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDied);

//레벨업 보상으로 고를 증강 선택지가 준비됐을 때
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnAugmentChoicesReady,
	const TArray<EAugmentID>&, Choices
);

//스태미나가 바뀌었을 때 현재 스태미나와 최대 스태미나 스태미나 게이지 UI용
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnPlayerStaminaChanged,
	float, CurrentStamina,
	float, MaxStamina
);

UCLASS()
class DREAMVEIL_API AMainPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:

	AMainPlayerCharacter();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	USpringArmComponent* SpringArmComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UCameraComponent* CameraComp;

	//스탯 컴포넌트 체력 공격력 방어력과 사망 이벤트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
	TObjectPtr<UCombatStatsComponent> CombatStats;

	//증강 컴포넌트 보상 증강 뽑기와 적용
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Augment")
	TObjectPtr<UDispatchTableComponent> DispatchTable;

	//인벤토리 컴포넌트 무기 파츠와 꿈의 조각 상점 강화 장착
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UInventoryComponent> Inventory;

	//1번 무기 권총 처음부터 가지고 있음 붙일 소켓은 블루프린트 Details의 Parent Socket에서 바꿈
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UWeaponBase> PistolWeapon;

	//2번 무기 소총 AcquireWeapon으로 얻기 전까지 숨겨져 있고 바꿀 수 없음
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UWeaponBase> RifleWeapon;

	//받은 데미지를 증강 라이브러리로 넘김 이게 없으면 체력이 안 깎임
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	// 무기

	//무기가 바뀌었을 때 이벤트 무기 UI 갱신용
	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnWeaponChanged OnWeaponChanged;

	//무기를 얻음 상점 인벤토리 몬스터 드랍에서 부를 것 얻기만 하고 바로 들지는 않음
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void AcquireWeapon(EWeaponSlot Slot);

	//가지고 있는 무기인지
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool HasWeapon(EWeaponSlot Slot) const;

	//가지고 있는 무기 전부 레벨을 넘길 때 GameInstance가 저장했다가 새 레벨의 플레이어에게 다시 줌
	const TArray<EWeaponSlot>& GetAcquiredWeaponSlots() const;

	//무기를 바꿔 듦 가지고 있지 않은 무기면 실패
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool EquipWeapon(EWeaponSlot Slot);

	//지금 들고 있는 무기 슬롯 로비처럼 싸우지 않는 곳이면 맨손을 뜻하는 Nothing
	//애님 블루프린트가 이 값으로 자세를 고름 Nothing이면 Blend Poses의 Default Pose(맨손 Idle)가 재생됨
	UFUNCTION(BlueprintPure, Category = "Weapon")
	EWeaponSlot GetCurrentWeaponSlot() const;

	//지금 들고 있는 무기
	UFUNCTION(BlueprintPure, Category = "Weapon")
	UWeaponBase* GetCurrentWeapon() const;

	//슬롯에 해당하는 무기 컴포넌트
	//인벤토리가 파츠를 어느 무기에 넘길지 찾을 때도 써서 private에서 public으로 옮김
	UWeaponBase* GetWeaponInSlot(EWeaponSlot Slot) const;

	// 사망

	//플레이어가 죽었을 때 이벤트 게임 오버 처리와 UI는 이걸 받는 쪽(GameState)이 함
	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnPlayerDied OnPlayerDied;

	// 스태미나

	//스태미나 변화 이벤트 스태미나 게이지 UI가 받을 것
	UPROPERTY(BlueprintAssignable, Category = "Stamina")
	FOnPlayerStaminaChanged OnStaminaChanged;

	//현재 스태미나 UI를 처음 띄울 때 한 번 읽는 용도
	UFUNCTION(BlueprintPure, Category = "Stamina")
	float GetCurrentStamina() const;

	//최대 스태미나 게이지 비율을 계산할 때 씀
	UFUNCTION(BlueprintPure, Category = "Stamina")
	float GetMaxStamina() const;

	//최대 스태미나를 늘리고 늘어난 만큼 채움 스태미나 증가 증강(UStaminaUpSkill)이 부름
	void IncreaseMaxStamina(float Amount);

	// 레벨

	//경험치 변화 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Level")
	FOnPlayerExperienceChanged OnExperienceChanged;

	//레벨 업 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Level")
	FOnPlayerLevelUp OnLevelUp;

	//현재 플레이어 레벨 1부터 시작
	//AActor에 월드 레벨을 돌려주는 GetLevel이 이미 있어서 이름을 GetPlayerLevel로 함
	UFUNCTION(BlueprintPure, Category = "Level")
	int32 GetPlayerLevel() const;

	//현재 레벨에서 모은 경험치
	UFUNCTION(BlueprintPure, Category = "Level")
	float GetCurrentExperience() const;

	//다음 레벨까지 필요한 경험치
	UFUNCTION(BlueprintPure, Category = "Level")
	float GetRequiredExperience() const;

	//경험치를 더함 필요한 만큼 모이면 레벨이 오르고 남은 경험치는 다음 레벨로 넘어감 죽은 상태면 무시
	UFUNCTION(BlueprintCallable, Category = "Level")
	void AddExperience(float Amount);

	//저장해둔 레벨과 경험치를 되돌림 GameInstance가 새 레벨의 플레이어에게 부름
	//맵을 넘기면 캐릭터가 새로 만들어져 레벨이 1로 돌아가는데 이걸로 이어붙임
	//증강 복원과 달리 레벨업 보상을 다시 주지 않음 보상은 이미 증강 기록으로 복원되기 때문
	void RestoreLevelProgress(int32 SavedLevel, float SavedExperience);

	// 레벨업 보상 증강 선택 UI가 씀

	//고를 증강 선택지가 준비됐을 때 이벤트 UI는 이걸 받아서 선택 창을 띄울 것
	UPROPERTY(BlueprintAssignable, Category = "Augment")
	FOnAugmentChoicesReady OnAugmentChoicesReady;

	//지금 떠 있는 선택지 UI를 늦게 열었을 때 다시 읽는 용도 없으면 빈 배열
	UFUNCTION(BlueprintPure, Category = "Augment")
	TArray<EAugmentID> GetCurrentAugmentChoices() const;

	//지금 고를 선택지가 있는지
	UFUNCTION(BlueprintPure, Category = "Augment")
	bool HasAugmentChoices() const;

	//지금 떠 있는 선택지 말고 뒤에 더 기다리는 보상 수 UI에 남은 횟수를 띄울 때 씀
	UFUNCTION(BlueprintPure, Category = "Augment")
	int32 GetPendingAugmentChoiceCount() const;

	//UI에서 고른 증강을 적용 지금 선택지에 없는 번호면 실패
	//한 번에 여러 레벨이 올랐으면 적용 뒤 다음 선택지 이벤트가 이어서 나감
	UFUNCTION(BlueprintCallable, Category = "Augment")
	bool SelectAugmentChoice(EAugmentID AugmentID);

	//증강 선택 보상을 하나 줌 레벨업 한 번과 같은 효과
	//몬스터가 떨군 증강 아이템을 주웠을 때 아이템 쪽에서 부를 것 죽은 상태면 무시
	UFUNCTION(BlueprintCallable, Category = "Augment")
	void GrantAugmentReward();

	// 테스트용 치트 콘솔(~)을 열고 함수 이름과 값을 입력해서 부름
	// 몬스터 보상 상점 UI가 아직 없어서 레벨업 무기 사망 흐름을 확인하는 용도

	//경험치를 넣어 레벨업과 증강 선택지를 확인 예) CheatAddExp 150
	UFUNCTION(Exec)
	void CheatAddExp(float Amount);

	//소총을 얻고 바로 들게 함 1 2번 입력 에셋이 없어도 연사 테스트 가능 예) CheatAcquireRifle
	UFUNCTION(Exec)
	void CheatAcquireRifle();

	//떠 있는 증강 선택지 중 하나를 고름 번호는 0부터 예) CheatPickAugment 0
	UFUNCTION(Exec)
	void CheatPickAugment(int32 ChoiceIndex);

	//자기 자신에게 데미지 사망 흐름 확인용 예) CheatDamageMe 9999
	UFUNCTION(Exec)
	void CheatDamageMe(float Amount);

	//지금 상태를 로그로 출력 레벨 경험치 체력 공방 무기 선택지 예) CheatShowStatus
	UFUNCTION(Exec)
	void CheatShowStatus();

protected:
	//무기별 발사 몽타주 기존 애님 블루프린트의 발사 슬롯으로 재생
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	TObjectPtr<UAnimMontage> PistolFireMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	TObjectPtr<UAnimMontage> RifleFireMontage;

	//로비에서만 쓸 애님 블루프린트 비워두면 평소 애님을 그대로 씀
	//로비용 캐릭터 블루프린트를 따로 만들지 않으려고 여기서 갈아끼움
	//BeginPlay에서 로비일 때만 SetAnimInstanceClass로 바꾸므로 레벨에서는 건드리지 않음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Animation")
	TSubclassOf<UAnimInstance> LobbyAnimClass;

	//체력이 위험할 때 반복 재생할 심장 소리 비워두면 소리 없이 넘어감
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase> HeartbeatSound;

	float SprintSpeed;
	float NoramalSpeed;
	float SprintSpeedMultiplier;

	//달리기 입력을 누르고 있는지 달리는 동안에는 총을 쏘지 않음
	bool bIsSprinting = false;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	//컨트롤러가 붙을 때 카메라 위아래 각도 제한을 검
	virtual void NotifyControllerChanged() override;

	void MovePlayer(const FInputActionValue& value);
	void StartJump(const FInputActionValue& value);
	void StopJump(const FInputActionValue& value);
	void Look(const FInputActionValue& value);
	void StartSprint(const FInputActionValue& value);
	void StopSprint(const FInputActionValue& value);

	//사격 입력을 누른 순간 단발 연사 상관없이 한 발
	void FireWeapon(const FInputActionValue& value);

	//사격 입력을 누르고 있는 동안 연사 무기만 계속 쏨
	void FireWeaponHeld(const FInputActionValue& value);

	//숫자 1 권총으로 바꿈
	void EquipPistolInput(const FInputActionValue& value);

	//숫자 2 소총으로 바꿈
	void EquipRifleInput(const FInputActionValue& value);

	//1레벨에서 2레벨로 갈 때 필요한 경험치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	float BaseRequiredExperience = 100.0f;

	//레벨이 하나 오를 때마다 필요한 경험치가 늘어나는 양
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	float RequiredExperienceGrowth = 50.0f;

private:
	//지금 들고 있는 무기 슬롯 생성자에서 권총으로 시작하고 로비면 BeginPlay에서 Nothing이 됨
	EWeaponSlot CurrentWeaponSlot;

	//가지고 있는 무기 슬롯 권총은 BeginPlay에서 넣음
	UPROPERTY(Transient)
	TArray<EWeaponSlot> AcquiredWeaponSlots;

	//현재 레벨
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Level", meta = (AllowPrivateAccess = "true"))
	int32 PlayerLevel = 1;

	//현재 레벨에서 모은 경험치
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Level", meta = (AllowPrivateAccess = "true"))
	float CurrentExperience = 0.0f;

	//지금 떠 있는 증강 선택지
	UPROPERTY(Transient)
	TArray<EAugmentID> CurrentAugmentChoices;

	// 상호작용 가능한 최대 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (AllowPrivateAccess = "true")
)
	float InteractionRadius = 250.0f;

	// 캐릭터 주변의 가장 가까운 액터와 상호작용
	void TryInteract(const FInputActionValue& Value);

	//아직 띄우지 못한 레벨업 보상 수 선택지를 고르는 동안 또 레벨이 오르면 쌓임
	int32 PendingAugmentChoiceCount = 0;

	//현재 스태미나 뛰는 동안 줄고 멈추면 잠깐 쉬었다가 다시 참
	float CurrentStamina;

	//최대 스태미나 기본값에서 시작해 스태미나 증가 증강을 얻을 때마다 늘어남
	float MaxStamina;

	//마지막으로 스태미나를 쓴 시각 이 뒤로 회복 대기 시간이 지나야 다시 차기 시작함
	//-100은 아직 한 번도 안 뛰었다는 안전값 회복 대기에 절대 걸리지 않게 넉넉히 옛날 시각으로 둠
	//지금은 안 뛰었으면 스태미나가 가득 차 있어서 차이가 없고 첫 달리기부터 실제 시각으로 덮어씀
	float LastStaminaUseTime = -100.0f;

	//스태미나를 줄이거나 채우는 반복 타이머 스태미나가 변하는 동안만 돌고 가득 차면 멈춤
	FTimerHandle StaminaTimerHandle;

	//이동 속도를 목표치까지 서서히 옮기는 타이머 속도가 목표에 닿으면 스스로 멈춤
	//바로 바꾸면 애니메이션 블렌드 스페이스가 뚝뚝 끊겨서 천천히 옮김
	FTimerHandle SpeedBlendTimerHandle;

	//지금 향해 가는 이동 속도 달리기를 켜고 끌 때마다 바뀜
	float TargetWalkSpeed;

	//심장 소리를 재생 중인 컴포넌트 재생 중이 아니면 nullptr
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> HeartbeatAudio;

	//들고 있는 무기만 보이고 나머지는 숨김
	void UpdateWeaponVisibility();

	//조준점을 계산해서 들고 있는 무기로 한 발 쏨 FireWeapon과 FireWeaponHeld가 같이 씀
	void FireCurrentWeapon();

	//쌓인 레벨업 보상이 있으면 다음 선택지를 뽑아 이벤트로 알림
	void DrawNextAugmentChoices();

	//달리기 상태를 바꾸고 이동 속도를 맞춤 달리기 입력과 스태미나 소진이 같이 씀
	void SetSprinting(bool bNewSprinting);

	//스태미나 타이머가 돌 때마다 불림 달리면 줄이고 쉬면 채움
	void UpdateStamina();

	//스태미나 값을 0과 최대치 사이로 바꾸고 UI에 알림
	void SetCurrentStamina(float NewStamina);

	//달리기 키를 누른 채 실제로 움직이고 있는지 스태미나 소모와 발사 금지가 같이 씀
	bool IsSprintMoving() const;

	//이동 속도를 목표치 쪽으로 한 칸 옮김 목표에 닿으면 타이머를 멈춤
	void UpdateWalkSpeedBlend();

	//목표 속도를 정하고 보간 타이머를 깨움 달리기를 켜고 끌 때 부름
	void SetTargetWalkSpeed(float NewTargetSpeed);

	//체력 비율을 보고 심장 소리를 켜거나 끔 체력이 바뀔 때마다 불림
	UFUNCTION()
	void UpdateHeartbeat(float OldValue, float NewValue);

	//스탯 컴포넌트의 사망 이벤트를 받음
	UFUNCTION()
	void HandleDead();
};
