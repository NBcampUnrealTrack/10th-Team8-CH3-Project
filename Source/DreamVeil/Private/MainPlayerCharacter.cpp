#include "MainPlayerCharacter.h"
#include "MainPlayerController.h"
#include "EnhancedInputComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AugmentDamageLibrary.h"
#include "CombatStatsComponent.h"
#include "DispatchTableComponent.h"
#include "DreamVeilGameInstance.h"
#include "Engine/DamageEvents.h"
#include "WeaponBase.h"


AMainPlayerCharacter::AMainPlayerCharacter()
{

	PrimaryActorTick.bCanEverTick = false;

	SpringArmComp = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArmComp->SetupAttachment(RootComponent);
	SpringArmComp->TargetArmLength = 300.0f;
	SpringArmComp->bUsePawnControlRotation = true;

	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	CameraComp->SetupAttachment(SpringArmComp, USpringArmComponent::SocketName);
	CameraComp->bUsePawnControlRotation = false;

	CombatStats = CreateDefaultSubobject<UCombatStatsComponent>(TEXT("CombatStats"));
	DispatchTable = CreateDefaultSubobject<UDispatchTableComponent>(TEXT("DispatchTable"));

	//스켈레톤에 무기 소켓이 아직 없어서 오른손 뼈에 붙임 소켓을 만들면 블루프린트 Parent Socket을 바꿀 것
	//기본 무기 권총은 UWeaponBase 그대로 씀
	PistolWeapon = CreateDefaultSubobject<UWeaponBase>(TEXT("PistolWeapon"));
	PistolWeapon->SetupAttachment(GetMesh(), TEXT("hand_r"));

	NoramalSpeed = 630.0f;
	SprintSpeedMultiplier = 1.7f;
	SprintSpeed = NoramalSpeed * SprintSpeedMultiplier;

	GetCharacterMovement()->MaxWalkSpeed = NoramalSpeed;
}


void AMainPlayerCharacter::BeginPlay()
{
	//컴포넌트 BeginPlay가 여기서 돌아서 빠지면 증강이 적용되지 않음
	Super::BeginPlay();

	CurrentWeapon = PistolWeapon;

	//이전 레벨에서 저장한 증강이 있으면 다시 적용 첫 레벨이면 아무 일도 없음
	if (UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>())
	{
		DreamVeilGameInstance->RestorePlayerAugments(DispatchTable);
	}
}

//받은 데미지를 증강 라이브러리로 넘김 방어력 체력 흡혈 가시 갑옷 처리
float AMainPlayerCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Damage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (Damage <= 0.0f)
	{
		return 0.0f;
	}

	return UAugmentDamageLibrary::ProcessIncomingDamage(this, Damage, DamageEvent.DamageTypeClass, EventInstigator, DamageCauser);
}

//들고 있는 무기로 화면 가운데를 향해 쏨
void AMainPlayerCharacter::FireWeapon(const FInputActionValue& value)
{
	if (!CurrentWeapon || !CameraComp)
	{
		return;
	}

	//죽은 뒤에는 쏘지 않음
	if (CombatStats && CombatStats->IsDead())
	{
		return;
	}

	//카메라가 캐릭터 뒤에 있어서 총구에서 카메라 정면으로 쏘면 조준점과 어긋남
	//사거리 끝의 조준점을 먼저 구하고 총구에서 그 점을 향해 쏨
	const FVector MuzzleLocation = CurrentWeapon->GetMuzzleLocation();
	const FVector AimPoint = CameraComp->GetComponentLocation() + CameraComp->GetForwardVector() * CurrentWeapon->GetRange();
	const FVector FireDirection = (AimPoint - MuzzleLocation).GetSafeNormal();

	CurrentWeapon->Fire(MuzzleLocation, FireDirection);
}

//현재 플레이어 레벨
int32 AMainPlayerCharacter::GetPlayerLevel() const
{
	return PlayerLevel;
}

//현재 레벨에서 모은 경험치
float AMainPlayerCharacter::GetCurrentExperience() const
{
	return CurrentExperience;
}

//다음 레벨까지 필요한 경험치 레벨이 오를수록 늘어남
float AMainPlayerCharacter::GetRequiredExperience() const
{
	return FMath::Max(BaseRequiredExperience + RequiredExperienceGrowth * (PlayerLevel - 1), 1.0f);
}

//경험치를 더하고 필요한 만큼 모이면 레벨을 올림
void AMainPlayerCharacter::AddExperience(float Amount)
{
	if (Amount <= 0.0f)
	{
		return;
	}

	CurrentExperience += Amount;

	//한 번에 많이 받으면 여러 레벨이 오를 수 있음 남은 경험치는 다음 레벨로 넘어감
	while (CurrentExperience >= GetRequiredExperience())
	{
		CurrentExperience -= GetRequiredExperience();
		PlayerLevel++;

		OnLevelUp.Broadcast(PlayerLevel);
	}

	OnExperienceChanged.Broadcast(CurrentExperience, GetRequiredExperience());
}

void AMainPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AMainPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (AMainPlayerController* PlayerController = Cast<AMainPlayerController>(GetController()))
		{
			if (PlayerController->MoveAction)
			{
				EnhancedInput->BindAction(PlayerController->MoveAction, ETriggerEvent::Triggered, this, &AMainPlayerCharacter::MovePlayer);
			}
			if (PlayerController->JumpAction)
			{
				EnhancedInput->BindAction(PlayerController->JumpAction, ETriggerEvent::Triggered, this, &AMainPlayerCharacter::StartJump);

				EnhancedInput->BindAction(PlayerController->JumpAction, ETriggerEvent::Triggered, this, &AMainPlayerCharacter::StopJump);
			}
			if (PlayerController->LookAction)
			{
				EnhancedInput->BindAction(PlayerController->LookAction, ETriggerEvent::Triggered, this, &AMainPlayerCharacter::CameraLock);
			}
			if (PlayerController->SprintAction)
			{
				EnhancedInput->BindAction(PlayerController->SprintAction, ETriggerEvent::Triggered, this, &AMainPlayerCharacter::StartSprint);

				EnhancedInput->BindAction(PlayerController->SprintAction, ETriggerEvent::Triggered, this, &AMainPlayerCharacter::StopSprint);
			}
			if (PlayerController->FireAction)
			{
				//누르고 있는 동안 계속 불리고 실제 발사 간격은 무기의 FireInterval이 막음
				EnhancedInput->BindAction(PlayerController->FireAction, ETriggerEvent::Triggered, this, &AMainPlayerCharacter::FireWeapon);
			}
		}
	}
}

void AMainPlayerCharacter::MovePlayer(const FInputActionValue& value)
{
	if (!Controller) return;

	const FVector2D MoveInput = value.Get<FVector2D>();

	if (!FMath::IsNearlyZero(MoveInput.X))
	{
		AddMovementInput(GetActorForwardVector(), MoveInput.X);
	}
	if (!FMath::IsNearlyZero(MoveInput.Y))
	{
		AddMovementInput(GetActorRightVector(), MoveInput.Y);
	}
}

void AMainPlayerCharacter::StartJump(const FInputActionValue& value)
{
	if (value.Get<bool>())
	{
		Jump();
	}
}

void AMainPlayerCharacter::StopJump(const FInputActionValue& value)
{
	if (!value.Get<bool>())
	{
		StopJumping();
	}
}

void AMainPlayerCharacter::CameraLock(const FInputActionValue& value)
{
	FVector2D LookInput = value.Get<FVector2D>();

	AddControllerYawInput(LookInput.X);
	AddControllerPitchInput(LookInput.Y);
}

void AMainPlayerCharacter::StartSprint(const FInputActionValue& value)
{
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
	}
}

void AMainPlayerCharacter::StopSprint(const FInputActionValue& value)
{
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = NoramalSpeed;
	}
}

