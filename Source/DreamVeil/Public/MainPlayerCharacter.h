#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "WeaponBase.h"
#include "MainPlayerCharacter.generated.h"

class USpringArmComponent; 
class UCameraComponent;

struct FInputActionValue;

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

	TSubclassOf<AWeaponBase> PistolClass;
	TSubclassOf<AWeaponBase> RifleClass; // 라이플은 아직 구현 안함
	AWeaponBase* PostolInstance;
	AWeaponBase* RifleInstance;
	


};
