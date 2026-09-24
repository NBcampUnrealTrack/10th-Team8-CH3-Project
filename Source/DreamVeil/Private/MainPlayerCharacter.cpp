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
#include "Components/CapsuleComponent.h"
#include "AugmentDamageLibrary.h"
#include "CombatStatsComponent.h"
#include "DispatchTableComponent.h"
#include "InventoryComponent.h"
#include "DreamVeilGameInstance.h"
#include "Engine/DamageEvents.h"
#include "RifleWeapon.h"
#include "WeaponBase.h"
#include "AugmentTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "TimerManager.h"
#include "MonsterCollision.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

//카메라가 위아래로 돌 수 있는 최대 각도
//엔진 기본은 거의 90도라서 끝까지 내리면 카메라가 캐릭터 바로 위로 가고 조금만 움직여도 방향이 휙 뒤집힘
const float CAMERA_PITCH_MIN = -50.0f;
const float CAMERA_PITCH_MAX = 50.0f;

//스태미나 수치 런앤히트 템포 기준 약 4초 뛰고 1초 쉰 뒤 4초에 걸쳐 다시 참
//시작할 때 최대 스태미나 스태미나 증가 증강을 얻으면 MaxStamina가 여기서부터 늘어남
const float BASE_MAX_STAMINA = 100.0f;
//실제로 뛰는 동안 1초에 줄어드는 양
const float SPRINT_STAMINA_COST_PER_SECOND = 25.0f;
//쉬는 동안 1초에 차는 양
const float STAMINA_REGEN_PER_SECOND = 25.0f;
//마지막으로 뛴 뒤 회복이 시작되기까지 기다리는 시간 초 짧게 끊어 달리기를 반복해서 스태미나를 아끼는 걸 막음
const float STAMINA_REGEN_DELAY = 1.0f;
//달리기를 새로 시작하려면 최소 이만큼은 있어야 함 0 근처에서 달리기가 켜졌다 꺼졌다 반복하는 걸 막음
const float MIN_STAMINA_TO_SPRINT = 20.0f;
//스태미나 갱신 간격 초 매 프레임 Tick 대신 스태미나가 변할 때만 도는 타이머를 씀
const float STAMINA_UPDATE_INTERVAL = 0.05f;

//이동 속도를 목표치까지 옮기는 간격 초 스태미나와 같은 간격이라 체감이 맞음
const float SPEED_BLEND_INTERVAL = 0.02f;

//1초에 바뀔 수 있는 이동 속도 값이 클수록 빨리 최고 속도에 도달함
//630에서 1071까지 441 차이라 900이면 약 0.5초에 걸쳐 올라감
const float SPEED_BLEND_RATE = 900.0f;

//이 차이보다 가까우면 목표에 닿은 것으로 보고 타이머를 멈춤
const float SPEED_BLEND_TOLERANCE = 1.0f;

//체력이 이 비율 아래로 내려가면 심장 소리를 재생함
const float HEARTBEAT_HEALTH_RATIO = 0.3f;

//조준점이 총구에서 이보다 가까우면 방향을 믿지 않고 카메라 정면으로 쏨
//벽에 붙었을 때 조준점이 총구 코앞에 잡혀 방향이 크게 튀는 것을 막음
const float MIN_AIM_DISTANCE = 150.0f;

//총구에서 조준점으로 가는 방향이 카메라가 보는 쪽과 이 각도보다 벌어지면 카메라 정면으로 쏨
//값을 키우면 더 관대해지고 줄이면 더 자주 정면으로 보정함
const float MAX_AIM_ANGLE_DEGREES = 35.0f;


AMainPlayerCharacter::AMainPlayerCharacter()
{
	static ConstructorHelpers::FObjectFinder<UAnimMontage> PistolFireAsset(
		TEXT("/Game/Animation/Pistol/MM_Pistol_Fire_Montage.MM_Pistol_Fire_Montage"));
	static ConstructorHelpers::FObjectFinder<UAnimMontage> RifleFireAsset(
		TEXT("/Game/Animation/Rifle/MM_Rifle_Fire_Montage.MM_Rifle_Fire_Montage"));
	PistolFireMontage = PistolFireAsset.Object;
	RifleFireMontage = RifleFireAsset.Object;


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
	Inventory = CreateDefaultSubobject<UInventoryComponent>(TEXT("Inventory"));

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

	//보간이 시작될 때 튀지 않게 지금 속도와 같은 값으로 시작
	TargetWalkSpeed = NoramalSpeed;

	//스태미나는 가득 찬 상태로 시작
	//레벨을 넘기면 캐릭터가 새로 만들어져 기본값으로 돌아가고 BeginPlay의 증강 복원이 스태미나 증가를 다시 적용함
	MaxStamina = BASE_MAX_STAMINA;
	CurrentStamina = MaxStamina;
}


void AMainPlayerCharacter::BeginPlay()
{
	//컴포넌트 BeginPlay가 여기서 돌아서 빠지면 증강이 적용되지 않음
	Super::BeginPlay();

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->SetCollisionObjectType(ECC_Pawn);
	Capsule->SetCollisionResponseToChannel(MonsterCollision::Monster, ECR_Block);
	Capsule->SetCollisionResponseToChannel(MonsterCollision::MonsterHitbox, ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(MonsterCollision::MonsterProjectile, ECR_Block);
	//기본 무기 권총은 처음부터 가지고 들고 시작
	AcquiredWeaponSlots.AddUnique(EWeaponSlot::Pistol);
	CurrentWeaponSlot = EWeaponSlot::Pistol;

	//로비에서는 싸우지 않으므로 맨손으로 바꿔서 시작
	//권총을 가진 기록(AcquiredWeaponSlots)은 그대로 두어서 레벨로 갈 때 다시 들 수 있음
	//맵마다 폰을 따로 지정하지 않고 캐릭터가 스스로 정함 게임모드나 World Settings를 건드릴 필요가 없음
	//이 한 줄이 무기 숨김 사격 금지 무기 교체 금지 맨손 애니메이션을 한꺼번에 정함
	if (const UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>())
	{
		if (DreamVeilGameInstance->IsInLobby())
		{
			CurrentWeaponSlot = EWeaponSlot::Nothing;
		}
	}

	UpdateWeaponVisibility();

	//로비에서는 총을 안 들었으므로 맨손 애님 블루프린트로 갈아끼움
	//로비용 캐릭터 블루프린트를 따로 만들지 않아도 되게 여기서 처리함
	//LobbyAnimClass를 비워두면 아무 일도 안 하고 평소 애님을 그대로 씀
	if (CurrentWeaponSlot == EWeaponSlot::Nothing && LobbyAnimClass && GetMesh())
	{
		GetMesh()->SetAnimInstanceClass(LobbyAnimClass);
	}

	//죽으면 입력을 막고 GameState 쪽에 알림
	if (CombatStats)
	{
		CombatStats->OnDead.AddDynamic(this, &AMainPlayerCharacter::HandleDead);

		//체력이 바뀔 때마다 심장 소리를 켤지 끌지 판단함
		CombatStats->OnCurrentHealthChanged.AddDynamic(this, &AMainPlayerCharacter::UpdateHeartbeat);
	}

	//이전 레벨에서 저장한 증강이 있으면 다시 적용 첫 레벨이면 아무 일도 없음
	if (UDreamVeilGameInstance* DreamVeilGameInstance = GetGameInstance<UDreamVeilGameInstance>())
	{
		DreamVeilGameInstance->RestorePlayerAugments(DispatchTable);

		//파츠와 꿈의 조각도 복원 무기가 이미 만들어진 뒤라 끼운 파츠가 바로 무기 수치에 들어감
		DreamVeilGameInstance->RestorePlayerInventory(Inventory);

		//레벨과 경험치도 복원 이게 없으면 맵을 넘길 때마다 레벨이 1로 돌아가서
		//HUD의 레벨 표시와 정예 몬스터 확률(GetEliteRate)이 매번 초기화됨
		DreamVeilGameInstance->RestorePlayerLevel(this);
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

	//선택 창을 띄운 채로 죽었으면 멈춘 게임을 풀어줌 안 풀면 게임 오버 화면에서 아무것도 못 함
	SetAugmentChoicePaused(false);

	//게임 오버 전달과 UI 갱신은 이 이벤트를 받는 쪽이 함
	//증강 인벤토리를 얼마나 남길지는 게임 오버 UI가 GameInstance의 ContinueAfterDeath를 부를 때 난이도로 정해짐 플레이어는 죽었다고 알리기만 함
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

//가지고 있는 무기 전부
//AcquiredWeaponSlots는 Transient라 새 레벨에서 캐릭터가 새로 만들어지면 권총만 남음 그래서 떠나기 전에 GameInstance가 이걸로 저장함
const TArray<EWeaponSlot>& AMainPlayerCharacter::GetAcquiredWeaponSlots() const
{
	return AcquiredWeaponSlots;
}

//무기를 바꿔 듦
bool AMainPlayerCharacter::EquipWeapon(EWeaponSlot Slot)
{
	//맨손 상태(로비)에서는 무기를 꺼내지 않음 숫자 1 2를 눌러도 바뀌지 않게 여기서 막음
	//들고 있는 칸이 Nothing인지만 보면 되어서 싸울 수 있는지를 따로 들고 있을 필요가 없음
	if (CurrentWeaponSlot == EWeaponSlot::Nothing)
	{
		return false;
	}

	//Nothing은 무기가 아니라 상태라서 이걸로 바꿔 드는 것도 막음 GetWeaponInSlot이 nullptr을 돌려줘서 아래에서 걸림
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
	//들고 있는 칸이 Nothing이면 어느 쪽과도 맞지 않아서 둘 다 숨겨짐
	//로비에서 맨손으로 보이는 처리가 따로 없이 이 규칙 하나로 끝남
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
	//맨손 상태(로비)면 GetCurrentWeapon이 nullptr이라 바로 아래에서 돌아감
	//보이지 않는 총이 소리와 이펙트를 내는 일을 따로 막을 필요가 없음
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

	//실제로 뛰는 중에는 쏘지 않음 달리기를 멈추면 누르고 있던 소총은 다음 프레임부터 다시 나감
	//단발 권총은 달리는 동안 누른 입력이 버려지므로 달리기를 멈춘 뒤 다시 눌러야 함
	//Shift만 누르고 제자리에 서 있으면 뛰는 게 아니므로 쏠 수 있음 스태미나 소모와 같은 기준(IsSprintMoving)을 씀
	if (IsSprintMoving())
	{
		return;
	}

	//연사 간격이 안 지났으면 조준 계산도 하지 않고 나감
	//소총을 누르고 있으면 매 프레임 여기로 오는데 대부분은 간격에 막히므로 카메라 트레이스를 아낌
	if (!CurrentWeapon->CanFire())
	{
		return;
	}

	FVector MuzzleLocation;
	FVector FireDirection;

	//조준 계산은 조준점 UI와 나눠 쓰는 함수가 함 위에서 무기를 이미 확인해서 여기서는 실패하지 않음
	if (!CalculateFireAim(MuzzleLocation, FireDirection))
	{
		return;
	}

	//여기부터는 무기 담당 총구 위치와 방향만 넘기면 나머지는 무기가 처리
	CurrentWeapon->Fire(MuzzleLocation, FireDirection);

	//연사 간격이나 달리기에 막힌 입력에서는 재생하지 않고 실제 발사마다 시작
	UAnimMontage* FireMontage = CurrentWeaponSlot == EWeaponSlot::Pistol
		? PistolFireMontage.Get() : RifleFireMontage.Get();
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (FireMontage)
		{
			AnimInstance->Montage_Play(FireMontage, 1.0f,
				EMontagePlayReturnType::MontageLength, 0.0f, false);
		}
	}

	//반동 :사격후 카메라를 위로 올림
	AddControllerPitchInput(-1.5f);
}

//총알이 나갈 총구 위치와 방향을 구함
//사격과 조준점 UI가 나눠 씀 둘이 따로 계산하면 화면의 조준점과 실제 탄착점이 어긋남
bool AMainPlayerCharacter::CalculateFireAim(FVector& OutMuzzleLocation, FVector& OutFireDirection) const
{
	//맨손 상태(로비)면 nullptr이라 조준할 것도 없음
	const UWeaponBase* CurrentWeapon = GetCurrentWeapon();

	if (!CurrentWeapon || !CameraComp)
	{
		return false;
	}

	OutMuzzleLocation = CurrentWeapon->GetMuzzleLocation();

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

	const FVector AimPoint = bAimHitSomething ? FVector(AimHit.ImpactPoint) : CameraTraceEnd;

	//총구에서 조준점으로 가는 방향 길이는 버리고 방향만 남김
	const FVector MuzzleToAim = AimPoint - OutMuzzleLocation;

	OutFireDirection = MuzzleToAim.GetSafeNormal();

	//벽에 바짝 붙으면 조준점이 총구 바로 옆이나 뒤에 잡혀서 총알이 엉뚱한 데로 나감
	//세 가지를 한꺼번에 걸러서 이럴 때는 그냥 카메라 정면으로 쏨
	// 1) 총구와 조준점이 같은 자리라 방향을 못 구한 경우
	// 2) 조준점이 총구에 너무 가까운 경우 거리가 짧으면 방향이 조금만 흔들려도 크게 튐
	// 3) 구한 방향이 카메라가 보는 쪽과 많이 벌어진 경우
	//예전에는 3)을 내적 0 이하(90도 넘게 벌어짐)로만 봤는데 벽에 붙으면 89도쯤에서도 이상하게 나가서 범위를 넓힘
	const bool bAimTooClose = MuzzleToAim.SizeSquared() < FMath::Square(MIN_AIM_DISTANCE);

	//둘 다 단위 벡터라 내적이 곧 두 방향 사이 각의 코사인 각이 클수록 코사인은 작아짐
	const bool bAimTooWide = FVector::DotProduct(OutFireDirection, CameraDirection) < FMath::Cos(FMath::DegreesToRadians(MAX_AIM_ANGLE_DEGREES));

	if (OutFireDirection.IsNearlyZero() || bAimTooClose || bAimTooWide)
	{
		OutFireDirection = CameraDirection;
	}

	return true;
}

//조준점을 그릴 화면 좌표
bool AMainPlayerCharacter::GetCrosshairScreenPosition(FVector2D& OutScreenPosition) const
{
	FVector MuzzleLocation;
	FVector FireDirection;

	if (!CalculateFireAim(MuzzleLocation, FireDirection))
	{
		return false;
	}

	//조준이 성공했으면 무기는 반드시 있음 사거리와 트레이스 채널을 물어보려고 다시 가져옴
	const UWeaponBase* CurrentWeapon = GetCurrentWeapon();

	//총구에서 실제로 쏠 방향으로 한 번 더 쏘아봐서 총알이 닿을 지점을 구함
	//카메라가 보는 곳을 그대로 쓰지 않는 이유 벽에 붙어서 방향이 재조정되면 카메라가 보는 곳과 탄착점이 달라짐
	const FVector TraceEnd = MuzzleLocation + FireDirection * CurrentWeapon->GetRange();

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	FHitResult Hit;
	const bool bHitSomething = GetWorld()->LineTraceSingleByChannel(
		Hit, MuzzleLocation, TraceEnd, CurrentWeapon->GetTraceChannel(), QueryParams
	);

	//아무것도 없으면 사거리 끝에 조준점을 둠 하늘을 봐도 조준점이 사라지지 않게
	const FVector CrosshairWorldLocation = bHitSomething ? FVector(Hit.ImpactPoint) : TraceEnd;

	//월드 좌표를 화면 좌표로 바꿈 탄착점이 화면 뒤나 밖이면 false가 돌아와서 UI가 조준점을 숨길 수 있음
	return UGameplayStatics::ProjectWorldToScreen(
		Cast<APlayerController>(GetController()), CrosshairWorldLocation, OutScreenPosition
	);
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

//저장해둔 레벨과 경험치를 되돌림
void AMainPlayerCharacter::RestoreLevelProgress(int32 SavedLevel, float SavedExperience)
{
	//1보다 작은 값이 들어와도 레벨이 0이 되지 않게 막음
	PlayerLevel = FMath::Max(SavedLevel, 1);
	CurrentExperience = FMath::Max(SavedExperience, 0.0f);

	//UI가 처음 뜰 때 옛 값을 보지 않게 바로 알림
	//레벨도 같이 알려야 함 예전에는 경험치만 알려서 맵을 넘길 때마다 화면의 레벨이 1로 굳어 있었음
	RefreshProgressUI();
}

//지금 레벨과 경험치를 UI에 다시 알림
void AMainPlayerCharacter::RefreshProgressUI()
{
	//레벨이 오른 게 아니어도 같은 이벤트로 알림 UI 입장에서는 둘 다 레벨 숫자를 새로 그리는 일이라 같음
	OnLevelUp.Broadcast(PlayerLevel);

	OnExperienceChanged.Broadcast(CurrentExperience, GetRequiredExperience());
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

//증강을 고르는 동안 게임을 멈추거나 푼다
void AMainPlayerCharacter::SetAugmentChoicePaused(bool bPaused)
{
	//이미 같은 상태면 아무것도 하지 않음 SetGamePaused를 겹쳐 불러도 되지만 의도를 분명히 하려고 막음
	if (bAugmentChoicePaused == bPaused)
	{
		return;
	}

	//멈추는 일은 컨트롤러가 함 SetGamePaused가 컨트롤러를 필요로 하고 누르고 있던 입력을 버리는 것도 컨트롤러만 할 수 있음
	//컨트롤러가 없으면 멈추지도 못하므로 상태도 바꾸지 않음
	AMainPlayerController* PlayerController = Cast<AMainPlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	PlayerController->SetGameSuspended(bPaused);

	bAugmentChoicePaused = bPaused;
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
			//고르는 동안 몬스터가 때리지 못하게 여기서 멈춤
			//UI가 어떻게 만들어졌든 상관없이 멈추도록 C++에서 처리함 위젯 쪽 배선에 기대지 않으려는 것
			//위젯은 멈춘 동안에도 입력을 받으므로 버튼은 그대로 눌림
			SetAugmentChoicePaused(true);

			OnAugmentChoicesReady.Broadcast(CurrentAugmentChoices);
			return;
		}
	}

	//풀이 비어서 뽑을 증강이 없으면 남은 보상은 버림
	CurrentAugmentChoices.Empty();

	//더 고를 게 없으니 멈춰둔 게임을 풀어줌
	SetAugmentChoicePaused(false);
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
	//스태미나가 거의 없으면 달리기를 시작하지 않음
	//다 쓴 뒤 키를 계속 누르고 있어도 다시 달리려면 조금 회복한 뒤 키를 다시 눌러야 함
	if (CurrentStamina < MIN_STAMINA_TO_SPRINT)
	{
		return;
	}

	SetSprinting(true);
}

void AMainPlayerCharacter::StopSprint(const FInputActionValue& value)
{
	SetSprinting(false);
}

//달리기 상태를 바꾸고 이동 속도를 맞춤
void AMainPlayerCharacter::SetSprinting(bool bNewSprinting)
{
	//이 값이 켜진 채 실제로 움직이면 FireCurrentWeapon에서 발사를 막음 (IsSprintMoving)
	bIsSprinting = bNewSprinting;

	//속도를 바로 바꾸지 않고 목표만 정함 실제 값은 UpdateWalkSpeedBlend가 조금씩 옮김
	//한 번에 바꾸면 이동 애니메이션(블렌드 스페이스)이 뚝 끊겨서 부자연스러움
	SetTargetWalkSpeed(bIsSprinting ? SprintSpeed : NoramalSpeed);

	//달리기를 시작하면 줄이기 시작하고 멈추면 회복을 시작해야 하므로 타이머가 쉬고 있으면 깨움
	if (!GetWorldTimerManager().IsTimerActive(StaminaTimerHandle))
	{
		GetWorldTimerManager().SetTimer(StaminaTimerHandle, this, &AMainPlayerCharacter::UpdateStamina, STAMINA_UPDATE_INTERVAL, true);
	}
}

//스태미나 타이머가 돌 때마다 불림
void AMainPlayerCharacter::UpdateStamina()
{
	const float CurrentTime = GetWorld()->GetTimeSeconds();

	//달리기 키를 누르고 실제로 움직이고 있을 때만 줄임 제자리에서 Shift만 누르고 있으면 안 줄어듦
	if (IsSprintMoving())
	{
		LastStaminaUseTime = CurrentTime;
		SetCurrentStamina(CurrentStamina - SPRINT_STAMINA_COST_PER_SECOND * STAMINA_UPDATE_INTERVAL);

		//다 쓰면 키를 누르고 있어도 강제로 걷게 함 이 순간부터 다시 총을 쏠 수 있음
		if (CurrentStamina <= 0.0f)
		{
			SetSprinting(false);
		}

		return;
	}

	//뛰다 멈춘 직후에는 잠깐 쉬었다가 참
	if (CurrentTime - LastStaminaUseTime < STAMINA_REGEN_DELAY)
	{
		return;
	}

	SetCurrentStamina(CurrentStamina + STAMINA_REGEN_PER_SECOND * STAMINA_UPDATE_INTERVAL);

	//가득 찼고 달리지도 않으면 더 할 일이 없으니 타이머를 멈춤 다음 달리기 때 SetSprinting이 다시 켬
	if (CurrentStamina >= MaxStamina && !bIsSprinting)
	{
		GetWorldTimerManager().ClearTimer(StaminaTimerHandle);
	}
}

//스태미나 값을 0과 최대치 사이로 바꾸고 UI에 알림
void AMainPlayerCharacter::SetCurrentStamina(float NewStamina)
{
	const float ClampedStamina = FMath::Clamp(NewStamina, 0.0f, MaxStamina);

	//값이 그대로면 UI에 알릴 필요 없음 가득 찬 채로 타이머가 돌 때 이벤트가 쏟아지지 않게
	if (FMath::IsNearlyEqual(ClampedStamina, CurrentStamina))
	{
		return;
	}

	CurrentStamina = ClampedStamina;

	OnStaminaChanged.Broadcast(CurrentStamina, MaxStamina);
}

//목표 속도를 정하고 보간 타이머를 깨움
void AMainPlayerCharacter::SetTargetWalkSpeed(float NewTargetSpeed)
{
	TargetWalkSpeed = NewTargetSpeed;

	//이미 돌고 있으면 그대로 두고 목표만 바뀜 달리다 말고 놓아도 중간부터 이어서 줄어듦
	if (!GetWorldTimerManager().IsTimerActive(SpeedBlendTimerHandle))
	{
		GetWorldTimerManager().SetTimer(SpeedBlendTimerHandle, this, &AMainPlayerCharacter::UpdateWalkSpeedBlend, SPEED_BLEND_INTERVAL, true);
	}
}

//이동 속도를 목표치 쪽으로 한 칸 옮김
void AMainPlayerCharacter::UpdateWalkSpeedBlend()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();

	if (!Movement)
	{
		GetWorldTimerManager().ClearTimer(SpeedBlendTimerHandle);
		return;
	}

	//이번 칸에 움직일 수 있는 최대량 FInterpConstantTo는 이 양만큼만 목표 쪽으로 옮겨줌
	//FInterpTo(지수 보간) 대신 쓰는 이유 지수 보간은 목표 근처에서 한없이 느려져서 최고 속도에 늦게 닿음
	Movement->MaxWalkSpeed = FMath::FInterpConstantTo(
		Movement->MaxWalkSpeed,
		TargetWalkSpeed,
		SPEED_BLEND_INTERVAL,
		SPEED_BLEND_RATE
	);

	//목표에 닿았으면 더 돌 이유가 없음 매 프레임 도는 Tick을 안 쓰려고 타이머를 멈춤
	if (FMath::IsNearlyEqual(Movement->MaxWalkSpeed, TargetWalkSpeed, SPEED_BLEND_TOLERANCE))
	{
		Movement->MaxWalkSpeed = TargetWalkSpeed;
		GetWorldTimerManager().ClearTimer(SpeedBlendTimerHandle);
	}
}

//체력 비율을 보고 심장 소리를 켜거나 끔
void AMainPlayerCharacter::UpdateHeartbeat(float OldValue, float NewValue)
{
	if (!HeartbeatSound || !CombatStats)
	{
		return;
	}

	//죽었으면 소리를 끄고 끝 죽은 뒤에도 두근거리면 이상함
	const bool bShouldPlay = !CombatStats->IsDead() && CombatStats->GetHealthPercentage() <= HEARTBEAT_HEALTH_RATIO;

	if (bShouldPlay)
	{
		//이미 재생 중이면 다시 틀지 않음 안 그러면 맞을 때마다 소리가 겹침
		if (!HeartbeatAudio)
		{
			//2D로 재생하는 이유 플레이어 자신의 심장이라 거리에 따라 작아지면 안 됨
			//사운드 에셋의 Looping을 켜두면 계속 반복됨
			HeartbeatAudio = UGameplayStatics::SpawnSound2D(this, HeartbeatSound);
		}

		return;
	}

	//체력을 회복했거나 죽었으면 소리를 멈춤
	if (HeartbeatAudio)
	{
		HeartbeatAudio->Stop();
		HeartbeatAudio = nullptr;
	}
}

//달리기 키를 누른 채 실제로 움직이고 있는지
//스태미나 소모(UpdateStamina)와 발사 금지(FireCurrentWeapon)가 같은 기준을 쓰도록 한 곳에 둠
//SizeSquared2D는 위아래(Z)를 뺀 수평 속도의 길이를 제곱한 값 0보다 큰지만 보면 되므로 제곱근 계산을 아낌
//수평만 보므로 제자리 점프는 뛰는 게 아니고 달리다 점프하면 공중에서도 뛰는 것으로 침
bool AMainPlayerCharacter::IsSprintMoving() const
{
	return bIsSprinting && GetVelocity().SizeSquared2D() > KINDA_SMALL_NUMBER;
}

//현재 스태미나
float AMainPlayerCharacter::GetCurrentStamina() const
{
	return CurrentStamina;
}

//최대 스태미나
float AMainPlayerCharacter::GetMaxStamina() const
{
	return MaxStamina;
}

//최대 스태미나를 늘리고 늘어난 만큼 채움 스태미나 증가 증강이 부름
//체력 증가 증강과 같은 규칙 최대치만 늘리면 게이지 비율이 갑자기 줄어 보여서 같이 채워줌
void AMainPlayerCharacter::IncreaseMaxStamina(float Amount)
{
	MaxStamina += Amount;

	//SetCurrentStamina가 새 최대치로 자르고 UI에 현재값과 새 최대치를 같이 알림
	//가득 찬 상태에서 얻어도 현재값이 늘어나므로 알림이 빠지지 않음
	SetCurrentStamina(CurrentStamina + Amount);
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
//뽑기를 거치지 않고 원하는 증강을 바로 얻음
void AMainPlayerCharacter::CheatGiveAugment(int32 AugmentID)
{
	if (!DispatchTable)
	{
		return;
	}

	//enum 범위를 벗어난 값이 들어오면 엉뚱한 스킬이 걸리므로 막음
	const int32 MaxAugmentID = static_cast<int32>(EAugmentID::ContinuousAttack);

	if (AugmentID < 0 || AugmentID > MaxAugmentID)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Cheat] AugmentID는 0~%d 사이여야 함"), MaxAugmentID);
		return;
	}

	const EAugmentID TargetAugment = static_cast<EAugmentID>(AugmentID);

	//ApplyAugment는 효과 적용과 기록 추가를 같이 함 반복 획득이 안 되는 증강은 풀에서도 빠짐
	const bool bApplied = DispatchTable->ApplyAugment(TargetAugment);

	UE_LOG(LogTemp, Warning, TEXT("[Cheat] %s 적용 %s"),
		*UDispatchTableComponent::GetAugmentDisplayName(TargetAugment).ToString(),
		bApplied ? TEXT("성공") : TEXT("실패"));
}

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
		*UEnum::GetValueAsString(CurrentWeaponSlot),
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
