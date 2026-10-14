#include "DreamObjectPoolSubsystem.h"
#include "DreamPoolable.h"
#include "Blueprint/UserWidget.h"
#include "Components/AudioComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/LatentActionManager.h"

void UDreamObjectPoolSubsystem::Deinitialize()
{
    for (UAudioComponent* Audio : AudioComponents) if (IsValid(Audio)) { Audio->Stop(); Audio->DestroyComponent(); }
    Widgets.Empty(); AudioComponents.Empty(); InactiveActors.Empty(); ManagedActors.Empty();
    PendingReusedActors.Empty(); InactiveSet.Empty(); Super::Deinitialize();
}
AActor* UDreamObjectPoolSubsystem::AcquireDeferred(UWorld* World, TSubclassOf<AActor> Class,
    const FTransform& Transform, AActor* Owner, APawn* Instigator, ESpawnActorCollisionHandlingMethod Collision)
{
    if (!World || !Class || Class->HasAnyClassFlags(CLASS_Abstract)) return nullptr;
    UDreamObjectPoolSubsystem* Pool = World->GetSubsystem<UDreamObjectPoolSubsystem>();
    if (Pool && Class->ImplementsInterface(UDreamPoolable::StaticClass()))
    {
        TArray<FDreamPooledActorEntry>& Bucket = Pool->InactiveActors.FindOrAdd(Class);
        for (int32 Index = Bucket.Num() - 1; Index >= 0; --Index)
        {
            AActor* Actor = Bucket[Index].Actor.Get();
            if (!IsValid(Actor)) { Bucket.RemoveAtSwap(Index); continue; }
            if (Bucket[Index].ReadyAt > World->GetTimeSeconds()) continue;
            if (Collision == ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding)
            {
                const UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
                if (Root)
                {
                    FCollisionQueryParams Query(SCENE_QUERY_STAT(PooledSpawn), false, Actor);
                    if (World->OverlapBlockingTestByChannel(Transform.GetLocation(), Transform.GetRotation(),
                        Root->GetCollisionObjectType(), Root->GetCollisionShape(), Query,
                        FCollisionResponseParams(Root->GetCollisionResponseToChannels()))) return nullptr;
                }
            }
            Bucket.RemoveAtSwap(Index); Pool->InactiveSet.Remove(Actor);
            Actor->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
            Actor->SetOwner(Owner); Actor->SetInstigator(Instigator); Actor->SetLifeSpan(0);
            IDreamPoolable* Reset = Cast<IDreamPoolable>(Actor);
            Reset->AdvancePoolGeneration(); Reset->PrepareForPoolReuse();
            if (Collision == ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn ||
                Collision == ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding)
            {
                FVector Location = Transform.GetLocation();
                UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Actor->GetRootComponent());
                const ECollisionEnabled::Type Before = Root ? Root->GetCollisionEnabled() : ECollisionEnabled::NoCollision;
                if (Root) Root->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
                Actor->SetActorEnableCollision(true);
                const bool bFound = World->FindTeleportSpot(Actor, Location, Transform.Rotator());
                Actor->SetActorEnableCollision(false);
                if (Root) Root->SetCollisionEnabled(Before);
                if (!bFound && Collision == ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding)
                { ReleaseOrDestroy(Actor); return nullptr; }
                Actor->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
            }
            Pool->PendingReusedActors.Add(Actor); ++Pool->ActorReuseCount;
            return Actor;
        }
    }
    AActor* Actor = World->SpawnActorDeferred<AActor>(Class, Transform, Owner, Instigator, Collision);
    if (Actor && Pool && Class->ImplementsInterface(UDreamPoolable::StaticClass()))
    {
        Pool->ManagedActors.Add(Actor); Cast<IDreamPoolable>(Actor)->AdvancePoolGeneration();
    }
    return Actor;
}
AActor* UDreamObjectPoolSubsystem::FinishAcquire(AActor* Actor, const FTransform& Transform)
{
    if (!IsValid(Actor)) return nullptr;
    UDreamObjectPoolSubsystem* Pool = Actor->GetWorld()->GetSubsystem<UDreamObjectPoolSubsystem>();
    if (Pool && Pool->PendingReusedActors.Remove(Actor))
    {
        Actor->SetActorHiddenInGame(false); Actor->SetActorEnableCollision(true);
        Actor->SetActorTickEnabled(Actor->GetClass()->GetDefaultObject<AActor>()->PrimaryActorTick.bStartWithTickEnabled);
        Cast<IDreamPoolable>(Actor)->OnPoolAcquired();
    }
    else Actor->FinishSpawning(Transform);
    return Actor;
}
AActor* UDreamObjectPoolSubsystem::Spawn(UWorld* World, TSubclassOf<AActor> Class, const FTransform& Transform,
    AActor* Owner, APawn* Instigator, ESpawnActorCollisionHandlingMethod Collision)
{
    return FinishAcquire(AcquireDeferred(World, Class, Transform, Owner, Instigator, Collision), Transform);
}
bool UDreamObjectPoolSubsystem::IsManaged(const AActor* Actor)
{
    const UDreamObjectPoolSubsystem* Pool = Actor && Actor->GetWorld() ? Actor->GetWorld()->GetSubsystem<UDreamObjectPoolSubsystem>() : nullptr;
    return Pool && Pool->ManagedActors.Contains(const_cast<AActor*>(Actor));
}
uint32 UDreamObjectPoolSubsystem::Generation(const AActor* Actor)
{
    const IDreamPoolable* Lifetime = Cast<IDreamPoolable>(Actor);
    return Lifetime ? Lifetime->GetPoolGeneration() : 0;
}
void UDreamObjectPoolSubsystem::ReleaseOrDestroy(AActor* Actor)
{
    if (!IsValid(Actor)) return;
    UDreamObjectPoolSubsystem* Pool = Actor->GetWorld()->GetSubsystem<UDreamObjectPoolSubsystem>();
    IDreamPoolable* Reset = Cast<IDreamPoolable>(Actor);
    if (!Pool || !Reset || !Pool->ManagedActors.Contains(Actor)) { Actor->Destroy(); return; }
    if (Pool->InactiveSet.Contains(Actor)) return;
    Pool->PendingReusedActors.Remove(Actor);
    Reset->OnPoolReturned();
    Actor->GetWorldTimerManager().ClearAllTimersForObject(Actor);
    Actor->GetWorld()->GetLatentActionManager().RemoveActionsForObject(Actor);
    Actor->SetLifeSpan(0); Actor->SetActorTickEnabled(false); Actor->SetActorEnableCollision(false);
    Actor->SetActorHiddenInGame(true); Actor->SetOwner(nullptr); Actor->SetInstigator(nullptr);
    TArray<FDreamPooledActorEntry>& Bucket = Pool->InactiveActors.FindOrAdd(Actor->GetClass());
    Bucket.RemoveAll([](const FDreamPooledActorEntry& E) { return !E.Actor.IsValid(); });
    if (Bucket.Num() >= Reset->GetPoolCapacity() || Pool->GetCachedActorCount() >= 256)
    {
        Pool->ManagedActors.Remove(Actor); Actor->Destroy(); return;
    }
    FDreamPooledActorEntry Entry; Entry.Actor = Actor;
    Entry.ReadyAt = Actor->GetWorld()->GetTimeSeconds() + Reset->GetPoolReuseDelay();
    Bucket.Add(Entry); Pool->InactiveSet.Add(Actor);
}
int32 UDreamObjectPoolSubsystem::GetCachedActorCount() const
{
    int32 Count = 0; for (const auto& Bucket : InactiveActors) Count += Bucket.Value.Num(); return Count;
}
UUserWidget* UDreamObjectPoolSubsystem::AcquireWidget(UObject* Context, TSubclassOf<UUserWidget> Class, APlayerController* Player)
{
    if (!Context || !Class || !Player) return nullptr;
    Widgets.RemoveAll([](const FDreamPooledWidgetEntry& E) { return !IsValid(E.Widget) || !E.Context.IsValid(); });
    for (FDreamPooledWidgetEntry& Entry : Widgets)
        if (Entry.bAvailable && Entry.Context.Get() == Context && Entry.Widget->GetClass() == Class)
        {
            Entry.bAvailable = false; Entry.Widget->SetVisibility(ESlateVisibility::Visible);
            ++WidgetReuseCount; return Entry.Widget;
        }
    UUserWidget* Widget = CreateWidget<UUserWidget>(Player, Class);
    if (Widget) { FDreamPooledWidgetEntry Entry; Entry.Widget = Widget; Entry.Context = Context; Widgets.Add(Entry); }
    return Widget;
}
bool UDreamObjectPoolSubsystem::ReleaseWidget(UUserWidget* Widget)
{
    for (int32 Index = 0; Index < Widgets.Num(); ++Index) if (Widgets[Index].Widget == Widget)
    { FDreamPooledWidgetEntry& Entry = Widgets[Index];
      if (Widgets.Num() > 512 && !Entry.bAvailable) { Widget->RemoveFromParent(); Widgets.RemoveAtSwap(Index); return true; }
      if (Entry.bAvailable) return true;
      Widget->RemoveFromParent(); Entry.bAvailable = true; Widget->SetVisibility(ESlateVisibility::Collapsed); return true;
    }
    return false;
}
UAudioComponent* UDreamObjectPoolSubsystem::PlaySound2D(USoundBase* Sound, bool bUISound, float Volume)
{
    if (!Sound) return nullptr;
    UAudioComponent* Audio = nullptr;
    for (UAudioComponent* Candidate : AudioComponents) if (IsValid(Candidate) && !Candidate->IsPlaying()) { Audio = Candidate; break; }
    if (!Audio)
    {
        if (AudioComponents.Num() >= 32) {
            UAudioComponent* Overflow = UGameplayStatics::CreateSound2D(GetWorld(), Sound, Volume, 1.0f, 0.0f, nullptr, false, true);
            if (Overflow) { Overflow->bIsUISound = bUISound; Overflow->Play(); } return Overflow; }
        Audio = UGameplayStatics::CreateSound2D(GetWorld(), Sound, Volume, 1.0f, 0.0f, nullptr, false, false);
        if (!Audio) return nullptr; AudioComponents.Add(Audio);
    }
    Audio->SetSound(Sound); Audio->SetVolumeMultiplier(Volume); Audio->bIsUISound = bUISound;
    Audio->Play(); return Audio;
}
