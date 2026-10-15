#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameFramework/Actor.h"
#include "AugmentTypes.h"
#include "DreamPoolable.h"
#include "DreamAugmentFeedback.generated.h"
class UStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USoundBase;
class USoundAttenuation;
class USoundConcurrency;
class ADreamAugmentCue;

UENUM(BlueprintType)
enum class EDreamAugmentCueShape : uint8 { Impact, Ring, Beam, Aura, Mark, Field, Barrier, Decoy, Acquire };

// Cosmetic only. No damage, movement, collision, navigation or gameplay timers are owned here.
UCLASS()
class DREAMVEIL_API UDreamAugmentFeedback : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    UDreamAugmentFeedback();
    UFUNCTION(BlueprintCallable,Category="Augment|Feedback",meta=(WorldContext="Source",AdvancedDisplay="Follow,End,TrackState"))
    static ADreamAugmentCue* Emit(AActor* Source,EAugmentID ID,EDreamAugmentCueShape Shape,const FVector& Point,
        float Radius=65,float Seconds=.45f,AActor* Follow=nullptr,const FVector& End=FVector::ZeroVector,bool TrackState=false);
    static void Acquired(AActor* Source,EAugmentID ID);
    static void StopSource(AActor* Source);
    static FLinearColor Color(EAugmentID ID);
    void SkillResult(AActor* Source,EAugmentID ID,bool Success);
    UFUNCTION(BlueprintPure,Category="Augment|Feedback") int32 GetActiveCueCount() const;
    FString Notice(AActor* Source) const;
    float FailureUntil(EAugmentID ID) const { return Failures.FindRef(ID); }
    virtual void Deinitialize() override;
private:
    UPROPERTY() TObjectPtr<UMaterialInterface> RingMaterial;
    UPROPERTY() TObjectPtr<UMaterialInterface> EnergyMaterial;
    UPROPERTY() TArray<TObjectPtr<USoundBase>> Sounds;
    UPROPERTY() TObjectPtr<USoundAttenuation> Attenuation;
    UPROPERTY() TObjectPtr<USoundConcurrency> Concurrency;
    TArray<TWeakObjectPtr<ADreamAugmentCue>> Cues;
    TMap<EAugmentID,float> SoundReady,ProcReady,Failures;
    float Window=-1,NoticeUntil=0;
    int32 WindowCount=0;
    FString NoticeText;
    TWeakObjectPtr<AActor> NoticeSource;
};

UCLASS(NotBlueprintable,Transient)
class DREAMVEIL_API ADreamAugmentCue : public AActor,public IDreamPoolable
{
    GENERATED_BODY()
public:
    ADreamAugmentCue();
    void Configure(AActor* Source,EAugmentID InID,EDreamAugmentCueShape InShape,const FVector& Point,
        const FVector& End,float InRadius,float Seconds,AActor* Follow,bool TrackState,
        UMaterialInterface* Ring,UMaterialInterface* Energy,float Opacity);
    void Refresh(float Seconds);
    bool Matches(AActor* Source,EAugmentID InID,EDreamAugmentCueShape InShape,AActor* Follow) const;
    bool IsPlaying() const { return bPlaying; }
    bool BelongsTo(AActor* Source) const { return Origin.Get()==Source; }
    virtual void Tick(float DeltaSeconds) override;
    virtual void PrepareForPoolReuse() override;
    virtual void OnPoolAcquired() override;
    virtual void OnPoolReturned() override;
    void ThreatTint(const FLinearColor& Color);
    bool bThreat=false;
    bool IsEssential() const { return bThreat||Shape==EDreamAugmentCueShape::Field||Shape==EDreamAugmentCueShape::Barrier||Shape==EDreamAugmentCueShape::Decoy; }
    virtual int32 GetPoolCapacity() const override { return 32; }
    virtual float GetPoolReuseDelay() const override { return 0.f; }
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> RingA;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> RingB;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Sparks;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RingMID;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> EnergyMID;
    TWeakObjectPtr<AActor> Origin,Attachment;
    uint32 OriginGeneration=0,AttachmentGeneration=0;
    EAugmentID ID=EAugmentID::Knockback;
    EDreamAugmentCueShape Shape=EDreamAugmentCueShape::Impact;
    FVector Location,Destination;
    float Started=0,Until=0,Radius=65,Strength=.7f;
    bool bPlaying=false,bTrackState=false;
    void UpdateVisual();
};
