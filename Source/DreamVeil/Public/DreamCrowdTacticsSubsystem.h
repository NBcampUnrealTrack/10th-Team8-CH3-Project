#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DreamCrowdTacticsSubsystem.generated.h"
class APawn;
class AMonsterBase;

// One shared, throttled wall probe instead of per-enemy/per-frame corner traces.
UCLASS()
class DREAMVEIL_API UDreamCrowdTacticsSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    bool IsCornered(APawn* Player,FVector& OpenDirection);
    bool FindApproach(AMonsterBase* Monster,APawn* Target,FVector& OutLocation);
private:
    TWeakObjectPtr<APawn> CachedPlayer;
    FVector StationaryOrigin=FVector::ZeroVector;
    FVector CachedDirection=FVector::ZeroVector;
    float StationarySince=0,NextProbe=0;
    bool bCorner=false;
    struct FApproach { FVector PlayerLocation,Location;float Expires=0;int32 Lane=INDEX_NONE; };
    int32 NextLane=0;
    TMap<TWeakObjectPtr<AMonsterBase>,FApproach> Approaches;
};
