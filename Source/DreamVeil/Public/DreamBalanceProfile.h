#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DreamBalanceProfile.generated.h"

// Shared, editor-adjustable numbers. Arrays retain the existing enum/stage order.
UCLASS(BlueprintType)
class DREAMVEIL_API UDreamBalanceProfile : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Difficulty")
    TArray<float> DifficultyStatScales = {1.0f,1.3f,1.5f,.85f};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Difficulty")
    TArray<float> EliteCurveScales = {1.6f,1.0f,.5f,2.0f};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage")
    TArray<float> StageStatScales = {1.0f,1.35f,1.8f,2.4f};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage")
    TArray<int32> StageMonsterLimits = {40,50,60,70};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage")
    TArray<float> StageSpawnIntervals = {3.0f,2.5f,2.0f,1.5f};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage")
    TArray<float> StageRangedShares = {.20f,.30f,.40f,.35f};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stage")
    TArray<int32> StageEliteLimits = {3,4,5,6};
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Endless", meta=(ClampMin="1"))
    int32 EndlessMonsterLimit=100;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Endless", meta=(ClampMin="0"))
    int32 EndlessEliteLimit=8;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Endless", meta=(ClampMin="0"))
    float EndlessStatGrowthPerMinute=.25f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Endless", meta=(ClampMin="0",ClampMax="1"))
    float EndlessRangedShare=.45f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0",ClampMax="1"))
    float SuperEasySpawnSkipChance=.10f;
};
