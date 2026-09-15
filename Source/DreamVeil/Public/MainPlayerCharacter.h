#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MainPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UCombatStatsComponent;
class UDispatchTableComponent;
class UWeaponBase;

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

	//권총 오른손에 붙음 붙일 소켓은 블루프린트 Details의 Parent Socket에서 바꿈
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UWeaponBase> PistolWeapon;

	//받은 데미지를 증강 라이브러리로 넘김 이게 없으면 체력이 안 깎임
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

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

	//경험치를 더함 필요한 만큼 모이면 레벨이 오르고 남은 경험치는 다음 레벨로 넘어감
	UFUNCTION(BlueprintCallable, Category = "Level")
	void AddExperience(float Amount);

protected:
	float SprintSpeed;
	float NoramalSpeed;
	float SprintSpeedMultiplier;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void MovePlayer(const FInputActionValue& value);
	void StartJump(const FInputActionValue& value);
	void StopJump(const FInputActionValue& value);
	void CameraLock(const FInputActionValue& value);
	void StartSprint(const FInputActionValue& value);
	void StopSprint(const FInputActionValue& value);

	//들고 있는 무기로 화면 가운데를 향해 쏨
	void FireWeapon(const FInputActionValue& value);

	//1레벨에서 2레벨로 갈 때 필요한 경험치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	float BaseRequiredExperience = 100.0f;

	//레벨이 하나 오를 때마다 필요한 경험치가 늘어나는 양
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	float RequiredExperienceGrowth = 50.0f;

private:
	//현재 들고 있는 무기 라이플이 생기면 1 2번 입력으로 바꿈
	UPROPERTY(Transient)
	TObjectPtr<UWeaponBase> CurrentWeapon;

	//현재 레벨
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Level", meta = (AllowPrivateAccess = "true"))
	int32 PlayerLevel = 1;

	//현재 레벨에서 모은 경험치
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Level", meta = (AllowPrivateAccess = "true"))
	float CurrentExperience = 0.0f;
};
