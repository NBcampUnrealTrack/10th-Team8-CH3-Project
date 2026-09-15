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
#include "RifleWeapon.h"
#include "WeaponBase.h"
#include "AugmentTypes.h"


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

	//소총은 얻기 전까지 숨겨둠 상점 인벤토리 드랍으로 얻으면 AcquireWeapon으로 해금
	RifleWeapon = CreateDefaultSubobject<URifleWeapon>(TEXT("RifleWeapon"));
	RifleWeapon->SetupAttachment(GetMesh(), TEXT("hand_r"));
	RifleWeapon->SetHiddenInGame(true);

	CurrentWeaponSlot = EWeaponSlot::Pistol;

	NoramalSpeed = 630.0f;
	SprintSpeedMultiplier = 1.7f;
	SprintSpeed = NoramalSpeed * SprintSpeedMultiplier;

	GetCharacterMovement()->MaxWalkSpeed = NoramalSpeed;
}


void AMainPlayerCharacter::BeginPlay()
{
	//컴포넌트 BeginPlay가 여기서 돌아서 빠지면 증강이 적용되지 않음
	Super::BeginPlay();

	//기본 무기 권총은 처음부터 가지고 들고 시작
	AcquiredWeaponSlots.AddUnique(EWeaponSlot::Pistol);
	CurrentWeaponSlot = EWeaponSlot::Pistol;
	UpdateWeaponVisibility();

	//죽으면 입력을 막고 GameState 쪽에 알림
	if (CombatStats)
	{
		CombatStats->OnDead.AddDynamic(this, &AMainPlayerCharacter::HandleDead);
	}

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

//플레이어가 죽었을 때
void AMainPlayerCharacter::HandleDead()
{
	//더 이상 조작하지 못하게 입력과 이동을 막음
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		DisableInput(PlayerController);
	}

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->DisableMovement();
	}

	//못 고른 레벨업 보상은 버림
	CurrentAugmentChoices.Empty();
	PendingAugmentChoiceCount = 0;

	//이번 판 증강 기록을 비움 게임 오버 뒤 레벨을 다시 로드해도 이전 판 증강이 남지 않게
	if (UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>())
	{
		DreamVeilGameInstance->ClearPlayerAugments();
	}

	//게임 오버 전달과 UI 갱신은 이 이벤트를 받는 쪽이 함
	OnPlayerDied.Broadcast();
}

//무기를 얻음
void AMainPlayerCharacter::AcquireWeapon(EWeaponSlot Slot)
{
	if (!GetWeaponInSlot(Slot))
	{
		return;
	}

	AcquiredWeaponSlots.AddUnique(Slot);
}

//가지고 있는 무기인지
bool AMainPlayerCharacter::HasWeapon(EWeaponSlot Slot) const
{
	return AcquiredWeaponSlots.Contains(Slot);
}

//무기를 바꿔 듦
bool AMainPlayerCharacter::EquipWeapon(EWeaponSlot Slot)
{
	if (!HasWeapon(Slot) || !GetWeaponInSlot(Slot))
	{
		return false;
	}

	//이미 들고 있는 무기면 할 일 없음
	if (CurrentWeaponSlot == Slot)
	{
		return true;
	}

	CurrentWeaponSlot = Slot;

	UpdateWeaponVisibility();

	OnWeaponChanged.Broadcast(CurrentWeaponSlot);

	return true;
}

//지금 들고 있는 무기 슬롯
EWeaponSlot AMainPlayerCharacter::GetCurrentWeaponSlot() const
{
	return CurrentWeaponSlot;
}

//지금 들고 있는 무기
UWeaponBase* AMainPlayerCharacter::GetCurrentWeapon() const
{
	return GetWeaponInSlot(CurrentWeaponSlot);
}

//슬롯에 해당하는 무기 컴포넌트 무기가 늘어나면 여기에 추가
UWeaponBase* AMainPlayerCharacter::GetWeaponInSlot(EWeaponSlot Slot) const
{
	switch (Slot)
	{
	case EWeaponSlot::Pistol:
		return PistolWeapon;
	case EWeaponSlot::Rifle:
		return RifleWeapon;
	default:
		return nullptr;
	}
}

//들고 있는 무기만 보이고 나머지는 숨김
void AMainPlayerCharacter::UpdateWeaponVisibility()
{
	if (PistolWeapon)
	{
		PistolWeapon->SetHiddenInGame(CurrentWeaponSlot != EWeaponSlot::Pistol);
	}

	if (RifleWeapon)
	{
		RifleWeapon->SetHiddenInGame(CurrentWeaponSlot != EWeaponSlot::Rifle);
	}
}

//숫자 1 권총으로 바꿈
void AMainPlayerCharacter::EquipPistolInput(const FInputActionValue& value)
{
	EquipWeapon(EWeaponSlot::Pistol);
}

//숫자 2 소총으로 바꿈 소총을 얻기 전에는 안 바뀜
void AMainPlayerCharacter::EquipRifleInput(const FInputActionValue& value)
{
	EquipWeapon(EWeaponSlot::Rifle);
}

//들고 있는 무기로 화면 가운데를 향해 쏨
void AMainPlayerCharacter::FireWeapon(const FInputActionValue& value)
{
	UWeaponBase* CurrentWeapon = GetCurrentWeapon();

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

	//죽은 뒤에 들어온 경험치는 무시
	if (CombatStats && CombatStats->IsDead())
	{
		return;
	}

	CurrentExperience += Amount;

	//한 번에 많이 받으면 여러 레벨이 오를 수 있음 남은 경험치는 다음 레벨로 넘어감
	while (CurrentExperience >= GetRequiredExperience())
	{
		CurrentExperience -= GetRequiredExperience();
		PlayerLevel++;

		//레벨마다 증강 선택 보상 하나
		PendingAugmentChoiceCount++;

		OnLevelUp.Broadcast(PlayerLevel);
	}

	OnExperienceChanged.Broadcast(CurrentExperience, GetRequiredExperience());

	//이미 선택 창이 떠 있으면 그걸 고른 뒤에 다음 선택지가 나감
	if (CurrentAugmentChoices.Num() == 0)
	{
		DrawNextAugmentChoices();
	}
}

//지금 떠 있는 증강 선택지
TArray<EAugmentID> AMainPlayerCharacter::GetCurrentAugmentChoices() const
{
	return CurrentAugmentChoices;
}

//지금 고를 선택지가 있는지
bool AMainPlayerCharacter::HasAugmentChoices() const
{
	return CurrentAugmentChoices.Num() > 0;
}

//UI에서 고른 증강을 적용
bool AMainPlayerCharacter::SelectAugmentChoice(EAugmentID AugmentID)
{
	//지금 띄운 선택지 중 하나만 고를 수 있음
	if (!CurrentAugmentChoices.Contains(AugmentID))
	{
		return false;
	}

	if (!DispatchTable || !DispatchTable->ApplyAugment(AugmentID))
	{
		return false;
	}

	CurrentAugmentChoices.Empty();

	//한 번에 여러 레벨이 올랐으면 다음 선택지를 이어서 띄움
	DrawNextAugmentChoices();

	return true;
}

//쌓인 레벨업 보상이 있으면 다음 선택지를 뽑아 이벤트로 알림
void AMainPlayerCharacter::DrawNextAugmentChoices()
{
	if (!DispatchTable)
	{
		return;
	}

	while (PendingAugmentChoiceCount > 0)
	{
		PendingAugmentChoiceCount--;

		if (DispatchTable->DrawAugmentChoices(CurrentAugmentChoices))
		{
			OnAugmentChoicesReady.Broadcast(CurrentAugmentChoices);
			return;
		}
	}

	//풀이 비어서 뽑을 증강이 없으면 남은 보상은 버림
	CurrentAugmentChoices.Empty();
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
				//누른 순간 점프 시작 뗀 순간 점프 멈춤 Triggered는 누르고 있는 동안에만 불려서 떼는 순간을 못 잡음
				EnhancedInput->BindAction(PlayerController->JumpAction, ETriggerEvent::Started, this, &AMainPlayerCharacter::StartJump);

				EnhancedInput->BindAction(PlayerController->JumpAction, ETriggerEvent::Completed, this, &AMainPlayerCharacter::StopJump);
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
			if (PlayerController->EquipPistolAction)
			{
				EnhancedInput->BindAction(PlayerController->EquipPistolAction, ETriggerEvent::Started, this, &AMainPlayerCharacter::EquipPistolInput);
			}
			if (PlayerController->EquipRifleAction)
			{
				EnhancedInput->BindAction(PlayerController->EquipRifleAction, ETriggerEvent::Started, this, &AMainPlayerCharacter::EquipRifleInput);
			}
		}
	}
}

//플레이어 앞뒤 양 옆으로 움직이기
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

//Started에 묶여서 누른 순간 한 번 불림
void AMainPlayerCharacter::StartJump(const FInputActionValue& value)
{
	Jump();
}

//Completed에 묶여서 뗀 순간 한 번 불림
void AMainPlayerCharacter::StopJump(const FInputActionValue& value)
{
	StopJumping();
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

