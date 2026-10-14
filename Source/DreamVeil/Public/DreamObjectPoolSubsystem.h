#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameFramework/Actor.h"
#include "DreamObjectPoolSubsystem.generated.h"
class UUserWidget;
class UPanelWidget;
class USoundBase;
class UAudioComponent;

USTRUCT()
struct FDreamPooledActorEntry
{
    GENERATED_BODY()
    TWeakObjectPtr<AActor> Actor;
    double ReadyAt = 0;
};
USTRUCT()
struct FDreamPooledWidgetEntry
{
    GENERATED_BODY()
    UPROPERTY(Transient) TObjectPtr<UUserWidget> Widget;
    TWeakObjectPtr<UObject> Context;
    bool bAvailable = false;
};

// Pools are owned by the world; travel releases every cached object and reference.
UCLASS()
class DREAMVEIL_API UDreamObjectPoolSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual void Deinitialize() override;
    static AActor* AcquireDeferred(UWorld* World, TSubclassOf<AActor> Class, const FTransform& Transform,
        AActor* Owner = nullptr, APawn* Instigator = nullptr,
        ESpawnActorCollisionHandlingMethod Collision = ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    static AActor* FinishAcquire(AActor* Actor, const FTransform& Transform);
    static AActor* Spawn(UWorld* World, TSubclassOf<AActor> Class, const FTransform& Transform,
        AActor* Owner = nullptr, APawn* Instigator = nullptr,
        ESpawnActorCollisionHandlingMethod Collision = ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    static void ReleaseOrDestroy(AActor* Actor);
    static bool IsManaged(const AActor* Actor);
    static uint32 Generation(const AActor* Actor);
    UUserWidget* AcquireWidget(UObject* Context, TSubclassOf<UUserWidget> Class, APlayerController* Player);
    bool ReleaseWidget(UUserWidget* Widget);
    UAudioComponent* PlaySound2D(USoundBase* Sound, bool bUISound, float Volume = 1.0f);
    UFUNCTION(BlueprintPure, Category="Pooling") int32 GetCachedActorCount() const;
    UFUNCTION(BlueprintPure, Category="Pooling") int32 GetActorReuseCount() const { return ActorReuseCount; }
    UFUNCTION(BlueprintPure, Category="Pooling") int32 GetWidgetReuseCount() const { return WidgetReuseCount; }
private:
    TMap<TSubclassOf<AActor>, TArray<FDreamPooledActorEntry>> InactiveActors;
    TSet<TWeakObjectPtr<AActor>> ManagedActors;
    TSet<TWeakObjectPtr<AActor>> PendingReusedActors;
    TSet<TWeakObjectPtr<AActor>> InactiveSet;
    UPROPERTY(Transient) TArray<FDreamPooledWidgetEntry> Widgets;
    UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> AudioComponents;
    int32 ActorReuseCount = 0;
    int32 WidgetReuseCount = 0;
};
