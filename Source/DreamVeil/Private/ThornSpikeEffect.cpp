#include "ThornSpikeEffect.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AThornSpikeEffect::AThornSpikeEffect()
{
	//가시 높이를 매 프레임 바꿔야 해서 Tick을 켬
	//0.5초쯤 살다 스스로 사라지므로 계속 도는 Tick이 아님
	PrimaryActorTick.bCanEverTick = true;

	SpikeMeshes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SpikeMeshes"));
	SetRootComponent(SpikeMeshes);

	//연출용이라 아무것도 막지 않음 가시가 캐릭터를 밀어내거나 총알을 막으면 안 됨
	SpikeMeshes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpikeMeshes->SetGenerateOverlapEvents(false);

	//그림자를 끄는 이유 0.5초만 있다 사라지는 연출인데 그림자까지 그리면 비용만 듦
	SpikeMeshes->SetCastShadow(false);

	//엔진 기본 원뿔을 씀 뾰족한 쪽이 위라 그대로 가시가 됨
	//전용 메시가 생기면 블루프린트에서 Static Mesh만 바꾸면 됨
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMeshFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));

	if (ConeMeshFinder.Succeeded())
	{
		SpikeMeshes->SetStaticMesh(ConeMeshFinder.Object);
	}

	//엔진 기본 회색 머티리얼 돌이나 어둠 느낌을 내려면 블루프린트에서 Element 0을 바꾸면 됨
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SpikeMaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	if (SpikeMaterialFinder.Succeeded())
	{
		SpikeMeshes->SetMaterial(0, SpikeMaterialFinder.Object);
	}
}

void AThornSpikeEffect::BeginPlay()
{
	Super::BeginPlay();

	//원뿔 메시를 못 찾았으면 그릴 것이 없음 빈 액터가 남지 않게 바로 사라짐
	if (!SpikeMeshes->GetStaticMesh())
	{
		Destroy();

		return;
	}

	Spikes.Reserve(SpikeCount);

	//원을 가시 수만큼 고르게 나눈 뒤 각도를 조금씩 흔듦
	//완전히 무작위로 두면 한쪽에 몰려서 반대쪽이 비어 보임
	const float AngleStep = 360.0f / FMath::Max(SpikeCount, 1);

	for (int32 SpikeIndex = 0; SpikeIndex < SpikeCount; ++SpikeIndex)
	{
		const float Angle = AngleStep * SpikeIndex + FMath::FRandRange(-AngleStep * 0.3f, AngleStep * 0.3f);
		const FVector Direction = FRotator(0.0f, Angle, 0.0f).Vector();

		//반지름도 조금씩 다르게 전부 같은 거리면 동그란 울타리처럼 보임
		const float Radius = SpawnRadius * FMath::FRandRange(0.7f, 1.0f);

		//바깥으로 눕히는 축 가시가 놓인 방향과 직각이라 이 축으로 돌리면 바깥쪽으로 넘어감
		const FVector TiltAxis = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal();

		FThornSpikeState Spike;

		Spike.BaseTransform = FTransform(
			FQuat(TiltAxis, FMath::DegreesToRadians(FMath::FRandRange(0.0f, TiltAngle))),
			//시작은 땅속 올라올 높이만큼 미리 내려둠
			Direction * Radius - FVector(0.0f, 0.0f, RiseHeight),
			//굵기도 조금씩 다르게 전부 같으면 복사한 티가 남
			SpikeScale * FMath::FRandRange(0.8f, 1.2f));

		Spike.StartDelay = FMath::FRandRange(0.0f, MaxStartDelay);

		Spikes.Add(Spike);

		//처음에는 전부 땅속에 있음 Tick이 시간에 맞춰 올림
		SpikeMeshes->AddInstance(Spike.BaseTransform);
	}

	//마지막 가시까지 다 가라앉으면 스스로 사라짐
	//Tick에서 직접 Destroy를 부르지 않는 이유 수명 관리는 엔진에 맡기는 편이 안전함
	SetLifeSpan(MaxStartDelay + SpikeDuration);
}

void AThornSpikeEffect::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ElapsedTime += DeltaSeconds;

	for (int32 SpikeIndex = 0; SpikeIndex < Spikes.Num(); ++SpikeIndex)
	{
		const float SpikeTime = ElapsedTime - Spikes[SpikeIndex].StartDelay;

		//아직 솟을 차례가 아니면 땅속에 그대로 둠
		if (SpikeTime < 0.0f)
		{
			continue;
		}

		//0에서 1까지 이 가시가 얼마나 진행됐는지
		const float Alpha = FMath::Clamp(SpikeTime / FMath::Max(SpikeDuration, KINDA_SMALL_NUMBER), 0.0f, 1.0f);

		//앞쪽은 빠르게 솟고 뒤쪽은 천천히 가라앉음
		//오르내리는 속도가 같으면 찌른다는 느낌이 안 나고 숨 쉬는 것처럼 보임
		const float SafeRiseRatio = FMath::Clamp(RiseTimeRatio, KINDA_SMALL_NUMBER, 1.0f - KINDA_SMALL_NUMBER);

		const float HeightRatio = Alpha < SafeRiseRatio
			? FMath::InterpEaseOut(0.0f, 1.0f, Alpha / SafeRiseRatio, 2.0f)
			: FMath::InterpEaseIn(1.0f, 0.0f, (Alpha - SafeRiseRatio) / (1.0f - SafeRiseRatio), 2.0f);

		FTransform SpikeTransform = Spikes[SpikeIndex].BaseTransform;
		SpikeTransform.AddToTranslation(FVector(0.0f, 0.0f, RiseHeight * HeightRatio));

		//다시 그리기 표시는 마지막 가시에서만 켬 가시마다 켜면 한 프레임에 여러 번 다시 그림
		SpikeMeshes->UpdateInstanceTransform(
			SpikeIndex,
			SpikeTransform,
			false,
			SpikeIndex == Spikes.Num() - 1,
			true);
	}
}
