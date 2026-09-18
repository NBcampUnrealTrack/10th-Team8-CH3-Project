#include "MainPlayerCharacter.h"
#include "MainPlayerController.h"
#include "InteractableActorBase.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Math/NumericLimits.h"
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
#include "Camera/PlayerCameraManager.h"

//카메라가 위아래로 돌 수 있는 최대 각도
//엔진 기본은 거의 90도라서 끝까지 내리면 카메라가 캐릭터 바로 위로 가고 조금만 움직여도 방향이 휙 뒤집힘
const float CAMERA_PITCH_MIN = -50.0f;
const float CAMERA_PITCH_MAX = 50.0f;


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
	PistolWeapon->SetupAttachment(GetMesh(), TEXT("Weapon_r_Pistol"));

	//소총은 얻기 전까지 숨겨둠 상점 인벤토리 드랍으로 얻으면 AcquireWeapon으로 해금
	RifleWeapon = CreateDefaultSubobject<URifleWeapon>(TEXT("RifleWeapon"));
	RifleWeapon->SetupAttachment(GetMesh(), TEXT("Weapon_r_Rifle"));
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

//컨트롤러가 붙을 때 카메라 위아래 각도 제한을 검
//각도 제한은 컨트롤러의 카메라 매니저가 매 프레임 적용함 캐릭터가 따로 자르면 엔진 제한과 겹치므로 값만 바꿔줌
//BeginPlay 대신 여기서 하는 이유 캐릭터가 먼저 스폰되고 컨트롤러가 나중에 붙으면 BeginPlay 시점엔 컨트롤러가 없을 수 있음
void AMainPlayerCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	APlayerController* PlayerController = Cast<APlayerController>(GetController());

	if (!PlayerController || !PlayerController->PlayerCameraManager)
	{
		return;
	}

	PlayerController->PlayerCameraManager->ViewPitchMin = CAMERA_PITCH_MIN;
	PlayerController->PlayerCameraManager->ViewPitchMax = CAMERA_PITCH_MAX;
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

//사격 입력을 누른 순간 단발 무기 연사 무기 상관없이 한 발
void AMainPlayerCharacter::FireWeapon(const FInputActionValue& value)
{
	FireCurrentWeapon();
}

//사격 입력을 누르고 있는 동안 매 프레임 불림 연사 무기만 계속 쏘고 단발 무기는 누른 순간 한 발로 끝
//누른 첫 프레임에는 FireWeapon과 같이 불리지만 무기의 FireInterval이 막아서 두 발이 나가지 않음
//단발 연사 구분은 무기가 아니라 여기서 함 누름과 유지를 구분하는 건 입력 시스템만 알기 때문
//무기는 bAutomatic 값과 발사 간격만 담당하므로 나중에 몬스터가 총을 써도 무기 코드는 그대로 쓰고 쏘는 주기만 AI에서 정하면 됨
void AMainPlayerCharacter::FireWeaponHeld(const FInputActionValue& value)
{
	UWeaponBase* CurrentWeapon = GetCurrentWeapon();

	//단발 무기면 누르고 있어도 더 쏘지 않음
	if (!CurrentWeapon || !CurrentWeapon->IsAutomatic())
	{
		return;
	}

	FireCurrentWeapon();
}

//들고 있는 무기로 화면 가운데를 향해 쏨
//여기서 하는 일은 조준까지 어디서 어느 쪽으로 쏠지만 정하고 마지막에 무기에게 넘김
//발사 뒤의 연사 간격 사격 트레이스 데미지 적중 증강은 전부 UWeaponBase::Fire가 함
//그래서 카메라 방식이 바뀌면 이 함수만 고치고 무기가 늘어나면 무기 클래스만 만들면 됨
void AMainPlayerCharacter::FireCurrentWeapon()
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

	//달리는 중에는 쏘지 않음 달리기를 멈추면 누르고 있던 소총은 다음 프레임부터 다시 나감
	//단발 권총은 달리는 동안 누른 입력이 버려지므로 달리기를 멈춘 뒤 다시 눌러야 함
	if (bIsSprinting)
	{
		return;
	}

	//연사 간격이 안 지났으면 조준 계산도 하지 않고 나감
	//소총을 누르고 있으면 매 프레임 여기로 오는데 대부분은 간격에 막히므로 카메라 트레이스를 아낌
	if (!CurrentWeapon->CanFire())
	{
		return;
	}

	const FVector MuzzleLocation = CurrentWeapon->GetMuzzleLocation();
	const FVector CameraLocation = CameraComp->GetComponentLocation();
	const FVector CameraDirection = CameraComp->GetForwardVector();
	const FVector CameraTraceEnd = CameraLocation + CameraDirection * CurrentWeapon->GetRange();

	//카메라와 총구 사이에 있는 내 캐릭터가 먼저 잡히지 않도록 제외
	FCollisionQueryParams AimQueryParams;
	AimQueryParams.AddIgnoredActor(this);

	//화면 가운데가 실제로 가리키는 지점을 카메라에서 먼저 찾음
	//사거리 끝을 그냥 조준점으로 쓰면 카메라가 캐릭터에 가깝거나 아래에 있을 때 총구 방향이 조준선과 어긋남
	FHitResult AimHit;
	const bool bAimHitSomething = GetWorld()->LineTraceSingleByChannel(
		AimHit, CameraLocation, CameraTraceEnd, CurrentWeapon->GetTraceChannel(), AimQueryParams
	);

	FVector AimPoint = bAimHitSomething ? AimHit.ImpactPoint : CameraTraceEnd;

	//벽에 바짝 붙으면 조준점이 총구보다 뒤에 잡힐 수 있음 그대로 쏘면 총알이 뒤로 날아감
	//내적이 0 이하면 총구에서 조준점으로 가는 방향이 카메라가 보는 쪽과 반대라는 뜻 즉 조준점이 뒤에 있음
	//이때는 조준점을 총구 앞쪽 사거리 끝으로 다시 잡아서 정면으로 쏨
	if (FVector::DotProduct(AimPoint - MuzzleLocation, CameraDirection) <= 0.0f)
	{
		AimPoint = MuzzleLocation + CameraDirection * CurrentWeapon->GetRange();
	}

	//총구에서 조준점으로 가는 방향 길이는 버리고 방향만 남김
	FVector FireDirection = (AimPoint - MuzzleLocation).GetSafeNormal();

	//총구와 조준점이 같은 자리면 방향을 못 구해서 0이 나옴 그때는 카메라 정면으로 쏨
	if (FireDirection.IsNearlyZero())
	{
		FireDirection = CameraDirection;
	}

	//여기부터는 무기 담당 총구 위치와 방향만 넘기면 나머지는 무기가 처리
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

//지금 떠 있는 선택지 말고 뒤에 더 기다리는 보상 수
int32 AMainPlayerCharacter::GetPendingAugmentChoiceCount() const
{
	return PendingAugmentChoiceCount;
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

//증강 선택 보상을 하나 줌
//레벨업과 같은 대기열(PendingAugmentChoiceCount)에 넣어서 레벨업 보상과 드롭 보상이 겹쳐도 하나씩 차례로 뜸
void AMainPlayerCharacter::GrantAugmentReward()
{
	//죽은 뒤에 주운 건 무시
	if (CombatStats && CombatStats->IsDead())
	{
		return;
	}

	PendingAugmentChoiceCount++;

	//이미 선택 창이 떠 있으면 그걸 고른 뒤에 다음 선택지가 나감 AddExperience와 같은 규칙
	if (CurrentAugmentChoices.Num() == 0)
	{
		DrawNextAugmentChoices();
	}
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
				EnhancedInput->BindAction(PlayerController->LookAction, ETriggerEvent::Triggered, this, &AMainPlayerCharacter::Look);
			}
			if (PlayerController->SprintAction)
			{
				EnhancedInput->BindAction(PlayerController->SprintAction, ETriggerEvent::Started, this, &AMainPlayerCharacter::StartSprint);

				EnhancedInput->BindAction(PlayerController->SprintAction, ETriggerEvent::Completed, this, &AMainPlayerCharacter::StopSprint);
			}
			if (PlayerController->FireAction)
			{
				//누른 순간 한 발 단발 연사 공통
				EnhancedInput->BindAction(PlayerController->FireAction, ETriggerEvent::Started, this, &AMainPlayerCharacter::FireWeapon);

				//누르고 있는 동안 매 프레임 불림 연사 무기만 쏘고 실제 발사 간격은 무기의 FireInterval이 막음
				EnhancedInput->BindAction(PlayerController->FireAction, ETriggerEvent::Triggered, this, &AMainPlayerCharacter::FireWeaponHeld);
			}
			if (PlayerController->EquipPistolAction)
			{
				EnhancedInput->BindAction(PlayerController->EquipPistolAction, ETriggerEvent::Started, this, &AMainPlayerCharacter::EquipPistolInput);
			}
			if (PlayerController->EquipRifleAction)
			{
				EnhancedInput->BindAction(PlayerController->EquipRifleAction, ETriggerEvent::Started, this, &AMainPlayerCharacter::EquipRifleInput);
			}
			if (PlayerController->InteractAction)
			{
				// E키를 누른 순간 상호작용
				EnhancedInput->BindAction(PlayerController->InteractAction, ETriggerEvent::Started, this, &AMainPlayerCharacter::TryInteract);
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

void AMainPlayerCharacter::Look(const FInputActionValue& value)
{
	FVector2D LookInput = value.Get<FVector2D>();

	AddControllerYawInput(LookInput.X);
	AddControllerPitchInput(LookInput.Y);
	//위아래 제한은 NotifyControllerChanged에서 카메라 매니저에 걸어둠 여기서는 입력만 넘김
}

void AMainPlayerCharacter::StartSprint(const FInputActionValue& value)
{
	//달리는 동안 FireCurrentWeapon에서 발사를 막음
	bIsSprinting = true;

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
	}
}

void AMainPlayerCharacter::StopSprint(const FInputActionValue& value)
{
	bIsSprinting = false;

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = NoramalSpeed;
	}
}

// 테스트용 치트 콘솔(~)에서 부름 몬스터 보상 상점 UI가 붙으면 지워도 됨

//경험치를 넣어 레벨업과 증강 선택지를 확인
void AMainPlayerCharacter::CheatAddExp(float Amount)
{
	AddExperience(Amount);

	UE_LOG(LogTemp, Warning, TEXT("[Cheat] Exp +%.0f -> Level %d (%.0f / %.0f), Choices %d"),
		Amount, PlayerLevel, CurrentExperience, GetRequiredExperience(), CurrentAugmentChoices.Num());
}

//소총을 얻고 바로 들게 함 1 2번 입력 에셋이 아직 없어도 연사를 테스트할 수 있게
void AMainPlayerCharacter::CheatAcquireRifle()
{
	AcquireWeapon(EWeaponSlot::Rifle);

	const bool bEquipped = EquipWeapon(EWeaponSlot::Rifle);

	UE_LOG(LogTemp, Warning, TEXT("[Cheat] Rifle acquired: %s, equipped: %s"),
		HasWeapon(EWeaponSlot::Rifle) ? TEXT("true") : TEXT("false"),
		bEquipped ? TEXT("true") : TEXT("false"));
}

//떠 있는 증강 선택지 중 하나를 고름
void AMainPlayerCharacter::CheatPickAugment(int32 ChoiceIndex)
{
	if (!CurrentAugmentChoices.IsValidIndex(ChoiceIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat] No choice at index %d (choices: %d)"), ChoiceIndex, CurrentAugmentChoices.Num());
		return;
	}

	const EAugmentID PickedAugmentID = CurrentAugmentChoices[ChoiceIndex];
	const bool bApplied = SelectAugmentChoice(PickedAugmentID);

	UE_LOG(LogTemp, Warning, TEXT("[Cheat] Augment %d applied: %s, next choices %d"),
		(int32)PickedAugmentID, bApplied ? TEXT("true") : TEXT("false"), CurrentAugmentChoices.Num());
}

//자기 자신에게 데미지 흡혈과 가시 갑옷은 자기 공격이라 걸리지 않음
void AMainPlayerCharacter::CheatDamageMe(float Amount)
{
	UAugmentDamageLibrary::ApplyAugmentDamageToTarget(this, this, Amount);

	if (!CombatStats)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Cheat] Damage %.0f -> Health %.0f / %.0f, Dead %s"),
		Amount, CombatStats->GetCurrentHealth(), CombatStats->GetMaxHealth(), CombatStats->IsDead() ? TEXT("true") : TEXT("false"));
}

//지금 상태를 로그로 출력
void AMainPlayerCharacter::CheatShowStatus()
{
	FString ChoiceText;

	for (EAugmentID ChoiceAugmentID : CurrentAugmentChoices)
	{
		ChoiceText += FString::Printf(TEXT("%d "), (int32)ChoiceAugmentID);
	}

	UE_LOG(LogTemp, Warning, TEXT("[Cheat] Level %d, Exp %.0f / %.0f, Weapon %s, Rifle owned %s"),
		PlayerLevel, CurrentExperience, GetRequiredExperience(),
		CurrentWeaponSlot == EWeaponSlot::Pistol ? TEXT("Pistol") : TEXT("Rifle"),
		HasWeapon(EWeaponSlot::Rifle) ? TEXT("true") : TEXT("false"));

	if (CombatStats)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat] Health %.0f / %.0f, Attack %.1f, Defence %.1f"),
			CombatStats->GetCurrentHealth(), CombatStats->GetMaxHealth(),
			CombatStats->GetAttackPower(), CombatStats->GetDefencePower());
	}

	UE_LOG(LogTemp, Warning, TEXT("[Cheat] Pending rewards %d, Choices now: %s"),
		PendingAugmentChoiceCount, ChoiceText.IsEmpty() ? TEXT("none") : *ChoiceText);
}

void AMainPlayerCharacter::TryInteract(const FInputActionValue& Value)
{
	UWorld* World = GetWorld();
	// 월드 확인
	if (!World)
	{
		return;
	}
	// 캐릭터 중심 위치
	const FVector InteractionCenter = GetActorLocation();
	// 주변에서 감지된 결과
	TArray<FOverlapResult> OverlapResults;
	// WorldDynamic 액터만 검색
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	// 플레이어 자신은 제외
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	// 캐릭터 주변 검색 범위
	const FCollisionShape InteractionShape = FCollisionShape::MakeSphere(InteractionRadius);
	// 캐릭터 주변 오브젝트 검색
	const bool bFoundActor = World->OverlapMultiByObjectType(OverlapResults, InteractionCenter, FQuat::Identity, ObjectQueryParams, InteractionShape, QueryParams);
	// 감지된 액터가 없으면 종료
	if (!bFoundActor)
	{
		return;
	}
	AInteractableActorBase* ClosestActor = nullptr;
	float ClosestDistanceSquared = TNumericLimits<float>::Max();
	// 가장 가까운 상호작용 액터 검색
	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AInteractableActorBase* InteractableActor = Cast<AInteractableActorBase>(OverlapResult.GetActor());
		// 일반 액터는 제외
		if (!InteractableActor)
		{
			continue;
		}
		// 캐릭터와 액터의 거리 계산
		const float DistanceSquared = FVector::DistSquared(InteractionCenter, InteractableActor->GetActorLocation());
		// 기존 대상보다 멀면 제외
		if (DistanceSquared >= ClosestDistanceSquared)
		{
			continue;
		}
		ClosestActor = InteractableActor;
		ClosestDistanceSquared = DistanceSquared;
	}
	// 상호작용 대상이 없으면 종료
	if (!ClosestActor)
	{
		return;
	}
	// 가장 가까운 액터와 상호작용
	ClosestActor->Interact(this);
}