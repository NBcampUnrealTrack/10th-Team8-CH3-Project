#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ThornSpikeEffect.generated.h"

class UInstancedStaticMeshComponent;

//가시 하나가 어디서 언제 솟는지
//높이는 시간으로 계산되므로 따로 들고 있지 않음
//USTRUCT으로 만들지 않은 이유 블루프린트에서 볼 일도 저장할 일도 없음
struct FThornSpikeState
{
	//땅속에 박혀 있을 때의 자리와 기울기 여기서 위로만 올라감
	FTransform BaseTransform;

	//이 가시가 솟기 시작하는 시각 초
	//전부 같은 순간에 솟으면 울타리가 통째로 올라오는 것처럼 보여서 조금씩 어긋나게 함
	float StartDelay = 0.0f;
};

//가시 갑옷 반사를 맞았을 때 발밑에서 솟았다가 가라앉는 가시
//Niagara로 만들지 않은 이유 원뿔 몇 개가 정해진 대로 오르내리는 게 전부라 파티클 시뮬레이션이 필요 없음
//전용 Niagara가 생기면 CombatStatsComponent의 ThornReflectEffect 칸에 넣으면 됨 그쪽이 있으면 그쪽을 씀
UCLASS()
class DREAMVEIL_API AThornSpikeEffect : public AActor
{
	GENERATED_BODY()

public:
	AThornSpikeEffect();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	//한 번에 솟는 가시 수
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thorn")
	int32 SpikeCount = 7;

	//가시가 솟는 원의 반지름 발밑 기준 캐릭터 캡슐 반지름보다 커야 몸을 뚫고 나오지 않음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thorn")
	float SpawnRadius = 75.0f;

	//가시가 솟아오르는 높이 이만큼 땅속에서 시작해서 이만큼 올라옴
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thorn")
	float RiseHeight = 130.0f;

	//가시 하나가 솟았다 다시 가라앉기까지 걸리는 시간 초
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thorn")
	float SpikeDuration = 0.45f;

	//전체 시간 중 솟는 데 쓰는 비율 나머지는 가라앉는 데 씀
	//작을수록 튀어나오듯 빠르게 솟고 천천히 내려감 0.5면 오르내리는 속도가 같아서 숨 쉬는 것처럼 보임
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thorn")
	float RiseTimeRatio = 0.25f;

	//마지막 가시가 솟기 시작할 때까지의 최대 지연 시간 초 0이면 전부 동시에 솟음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thorn")
	float MaxStartDelay = 0.12f;

	//가시 하나의 굵기와 길이 엔진 기본 원뿔이 100cm라 1이면 100cm
	//가로를 얇게 세로를 길게 두어야 가시처럼 보임
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thorn")
	FVector SpikeScale = FVector(0.22f, 0.22f, 1.1f);

	//가시가 바깥으로 눕는 최대 각도 0이면 전부 똑바로 섬
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thorn")
	float TiltAngle = 12.0f;

private:
	//가시 전부를 한 컴포넌트에 그림 컴포넌트를 가시 수만큼 만들면 드로우 콜만 늘어남
	UPROPERTY(VisibleAnywhere, Category = "Thorn")
	TObjectPtr<UInstancedStaticMeshComponent> SpikeMeshes;

	//가시마다의 자리와 시작 시각 인덱스가 곧 인스턴스 번호
	TArray<FThornSpikeState> Spikes;

	//생긴 뒤로 흐른 시간
	float ElapsedTime = 0.0f;
};
