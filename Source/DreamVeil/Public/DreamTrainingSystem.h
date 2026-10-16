#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/SaveGame.h"
#include "InteractableActorBase.h"
#include "AugmentTypes.h"
#include "DreamTrainingSystem.generated.h"
class AMainPlayerCharacter;
UENUM(BlueprintType)
enum class EDreamTrainingStat:uint8 { Health,Defence,Attack,Stamina };
UCLASS()
class DREAMVEIL_API UDreamTrainingSaveGame:public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY() TArray<int32> Levels={0,0,0,0};
    UPROPERTY() int32 RemovalCount=0;
};
UCLASS()
class DREAMVEIL_API UDreamTrainingSystem:public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    UFUNCTION(BlueprintPure,Category="Training") int32 GetLevel(EDreamTrainingStat Stat) const;
    UFUNCTION(BlueprintPure,Category="Training") int32 GetUpgradeCost(EDreamTrainingStat Stat) const;
    UFUNCTION(BlueprintPure,Category="Training") int32 GetRemovalCost() const;
    UFUNCTION(BlueprintCallable,Category="Training") bool Upgrade(AMainPlayerCharacter* Player,EDreamTrainingStat Stat);
    UFUNCTION(BlueprintCallable,Category="Training") bool RemoveAugment(AMainPlayerCharacter* Player,EAugmentID ID);
    UFUNCTION(BlueprintPure,Category="Training") FText GetMessage() const { return Message; }
    void ApplyToPlayer(AMainPlayerCharacter* Player);
    static int32 CalculateUpgradeCost(int32 Level,int32 TotalLevels);
private:
    UPROPERTY() TObjectPtr<UDreamTrainingSaveGame> Progress;
    TMap<TWeakObjectPtr<AMainPlayerCharacter>,FVector4> Applied;
    FText Message;
    bool CanUse(AMainPlayerCharacter* Player) const;
    bool SaveProgress() const;
};
UCLASS()
class DREAMVEIL_API ADreamTrainingCabinet:public AInteractableActorBase
{
    GENERATED_BODY()
public:
    ADreamTrainingCabinet();
    virtual void Interact_Implementation(AActor* Interactor) override;
};
