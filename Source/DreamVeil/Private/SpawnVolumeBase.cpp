// Fill out your copyright notice in the Description page of Project Settings.


#include "SpawnVolumeBase.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "NavigationSystem.h"

// Sets default values
ASpawnVolumeBase::ASpawnVolumeBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	SpawnArea = CreateDefaultSubobject<UBoxComponent>(TEXT("Spawn Area"));
	SetRootComponent(SpawnArea);

	//레벨에 끌어다 놓자마자 쓸 수 있게 문 한 짝 폭 정도로 키워둠
	//기본값 32는 너무 작아서 뷰포트에서 잘 보이지도 않고 랜덤으로 뽑을 자리도 거의 한 점임
	SpawnArea->SetBoxExtent(FVector(200.0f, 200.0f, 100.0f));

	//부딪히는 물건이 아니라 "이 안에 낸다"는 범위 표시일 뿐이라 충돌을 끔
	//켜두면 몬스터나 총알이 이 상자에 걸려서 문 앞에서 이상하게 막히거나 튕김
	SpawnArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	//이 상자가 내비메시를 건드리지 못하게 막음
	//스폰할 자리를 찾는 데 내비메시를 쓰는데 자기가 그 자리를 깎아버리면 낼 곳이 사라짐
	SpawnArea->SetCanEverAffectNavigation(false);

	//어느 쪽이 앞인지 뷰포트에서 보이게 하는 화살표
	//문 앞에 놓을 때 이 화살표를 방 안쪽으로 돌려두면 몬스터가 문을 등지고 나옴
	//에디터 전용이라 게임에는 나오지 않음 UArrowComponent가 스스로 그렇게 정해 둠
	SpawnDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("Spawn Direction"));
	SpawnDirection->SetupAttachment(SpawnArea);
	SpawnDirection->SetArrowColor(FLinearColor::Red);
	SpawnDirection->ArrowSize = 2.0f;
}

// virtual로 순수 가상함수 만들라했더니 오류나서 이리 빈 상태로 둠
// 해당 클래스는 abstract, 곧 추상클래스긴 해서 딱히 상관 없을듯 구현만 해주면 됨
void ASpawnVolumeBase::ExecuteSpawnActor()
{
}

// Called when the game starts or when spawned
void ASpawnVolumeBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// bool로 성공 여부를 판단하고, 결과값은 뭐시기냐 저거 그 포인터랑 비슷한 참조 그걸로 반환
// 엠퍼센더 붙이면 뭐라했더라 기억이 안나구요
bool ASpawnVolumeBase::TryGetRandomNavLocation(FVector& OutLocation) const
{
	// SpawnArea, 곧 Box Component의 중심 좌표 가져옴
	const FVector Origin = SpawnArea->GetComponentLocation();
	// 중심부터 끝까지의 거리, 곧 반지름...이라고하면 이상하긴 한데 어쨌든 그런 개념
	// 걍 스케일 반영된 x,y,z 크기값의 절반 리턴해준다 보면 됨다
	const FVector Extent = SpawnArea->GetScaledBoxExtent();

	//NavMesh 위에 생성해야돼서 높이를 나타내는 Z값은 원본 그대로, 나머지 XY만랜덤값 먼저 뽑음
	const FVector RandomLocationWithoutZ(
		FMath::FRandRange(Origin.X - Extent.X, Origin.X + Extent.X),	// 범위 내의 X 랜덤값
		FMath::FRandRange(Origin.Y - Extent.Y, Origin.Y + Extent.Y),	// 범위 내의 Y 랜덤값
		Origin.Z	// 중심기준 높이 걍 그대로 대입
	);

	// 현재 월드 내비게이션 시스템 가져오기
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSystem) return false;

	// 결과값 돌려받을 변수. 이 안에 Location 들어있음
	FNavLocation NavLocation;
	
	// RandomLocationWithoutZ 안에서 NavMesh가 유효한 곳인지 탐색할 검색 박스의 스케일을 지정해줌. 여기서 스케일은 상단 SpawnArea의 GetScaledBoxExtent랑 같은 형태
	// 기본적으로 스폰볼륨은 Navigation 경로랑 겹치지 않을 수 있어서 100씩 여유를 주고,
	// 해당 볼륨만큼의 높이를 지정해줌으로 탐색 가능
	const FVector SearchExtent(100.0f, 100.0f, Extent.Z);

	// 랜덤 좌표로 뽑은 포인트가 Navigation 경로로 쓸 수 있는지 바로 위에 지정한 검색 박스 범위를 두어 검색해보고,
	// 적절한 포인트를 탐색 시도해보고 안되면 false 리턴
	if (!NavSystem->ProjectPointToNavigation(
		RandomLocationWithoutZ,
		NavLocation,
		SearchExtent
	))
	{
		return false;
	}

	// 성공하면 로케이션만 뽑아 저장하고 true 리턴
	OutLocation = NavLocation.Location;
	return true;
}

