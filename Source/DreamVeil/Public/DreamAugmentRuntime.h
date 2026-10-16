#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Pawn.h"
#include "AugmentTypes.h"
#include "DreamPoolable.h"
#include "TimerManager.h"
#include "DreamAugmentRuntime.generated.h"
class UDispatchTableComponent;
class AMainPlayerCharacter;
class UCombatStatsComponent;
class USphereComponent;
class UBoxComponent;
class UStaticMeshComponent;
class UDecalComponent;
class AMonsterBase;

UCLASS()
class DREAMVEIL_API UDreamAugmentStatus : public UActorComponent
{
    GENERATED_BODY()
public:
    UDreamAugmentStatus();
    static UDreamAugmentStatus* For(AActor* Actor);
    bool Slow(FName Source,float Amount,float Duration);
    void Seal(float Duration);
    bool IsSealed() const;
    void Reset();
    void PullToward(const FVector& Point,float MaxDistance);
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    struct FSlow { float Amount=0,Until=0; };
    TMap<FName,FSlow> Slows;
    float BaseSpeed=0,AppliedSpeed=-1,SealUntil=0;
    FTimerHandle Timer;
    void Update();
};

UCLASS()
class DREAMVEIL_API ADreamAugmentField : public APawn,public IDreamPoolable
{
    GENERATED_BODY()
public:
    ADreamAugmentField();
    // 0 slow zone, 1 trap, 2 decoy, 3 projectile barrier.
    void Configure(APawn* Source,int32 Kind,float Radius,float Duration,float Strength);
    static APawn* FindDecoyFor(const AMonsterBase* Monster,APawn* Fallback);
    static void RefreshDecoyTargets(UWorld* World);
    virtual void PrepareForPoolReuse() override;
    virtual void OnPoolAcquired() override;
    virtual void OnPoolReturned() override;
    virtual int32 GetPoolCapacity() const override { return 12; }
    virtual float GetPoolReuseDelay() const override { return .25f; }
    virtual float TakeDamage(float Damage,const FDamageEvent& Event,AController* Instigator,AActor* Causer) override;
    void Expire();
private:
    UPROPERTY() TObjectPtr<USphereComponent> Sphere;
    UPROPERTY() TObjectPtr<UBoxComponent> Barrier;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY() TObjectPtr<UDecalComponent> Warning;
    TWeakObjectPtr<APawn> Source;
    int32 Kind=0;
    float Radius=200,Strength=.15f,Until=0,Health=20;
    FTimerHandle Timer;
    void Update();
};

UCLASS()
class DREAMVEIL_API UDreamAugmentRuntime : public UObject
{
    GENERATED_BODY()
public:
    virtual UWorld* GetWorld() const override;
    void Start();
    void Reset();
    UDispatchTableComponent* Dispatch() const;
    AMainPlayerCharacter* Player() const;
    UCombatStatsComponent* Stats() const;
    float Now() const;
    bool Has(EAugmentID ID) const;
    float Scale(EAugmentID ID,float PerStack,float Cap) const;
    float Power() const;
    float Duration() const;
    float Healing() const;
    float FireRate() const;
    float Recoil() const;
    float GetCost(EAugmentID ID) const;
    float GetCooldown(EAugmentID ID) const;
    float CooldownRemaining(EAugmentID ID) const;
    FString SkillState(EAugmentID ID) const;
    bool IsActive(EAugmentID ID) const;
    bool HasFeedbackState(EAugmentID ID) const;
    bool BlocksFire() const;
    bool AllowsSprintFire() const;
    void UnlockSkill(EAugmentID ID);
    bool Use(EAugmentID ID);
    bool BeginShot(const FVector& Start,const FVector& Direction,float Damage,float Range,ECollisionChannel Channel);
    void EndShot();
    float ModifyDirectDamage(AActor* Target,float Damage,bool Headshot);
    void AfterDirectHit(AActor* Target,float BaseDamage,bool Killed,const FHitResult& Hit);
    void AfterHealthDamage(AActor* Attacker,float Actual,bool Fire);
    float AbsorbDamage(float FinalDamage,bool Projectile,bool Fire);
    void Burn(AActor* Target,float BaseDamage,float Seconds);
    float VampireRatio() const;
    float ThornsMultiplier();
    bool IsFireImmune() const;
    float ShieldAmount() const { return Shield; }
    int32 CombatReward(int32 Base);
    int32 ShotNumber=0;
    bool bDirectDamageContext=false;
    EAugmentID ShotSkill=static_cast<EAugmentID>(255);
private:
    TMap<EAugmentID,float> Ready,Active;
    TMap<EAugmentID,int32> Charges;
    struct FTargetState { int32 Generation=0,Hits=0;float Last=0,MarkUntil=0;bool EmpoweredMark=false; };
    TMap<TWeakObjectPtr<AActor>,FTargetState> Targets;
    struct FBurn { TWeakObjectPtr<AActor> Target;int32 Generation=0;float Until=0,Next=0,Damage=0; };
    TArray<FBurn> Burns;
    struct FFieldHandle { TWeakObjectPtr<ADreamAugmentField> Actor;int32 Generation=0; };
    TMap<int32,FFieldHandle> Fields;
    TWeakObjectPtr<AActor> FocusTarget;
    int32 FocusGeneration=0,FocusCount=0,RampageStacks=0,DirectHitCount=0;
    float FocusTime=0,LastDamage=-100,FireSince=-1,LastSprintStart=-1,LastSprintStop=-100;
    float Shield=0,ShieldUntil=0,FireImmunityUntil=0,RewardRemainder=0,ReflectNext=0;
    float BaseWalk=0,BaseSprint=0,BaseJump=0,FortressBonusApplied=0,RegenNext=0;
    FVector ReturnPoint=FVector::ZeroVector,ShotStart=FVector::ZeroVector,ShotDirection=FVector::ForwardVector;
    EAugmentID Prepared=static_cast<EAugmentID>(255);
    float PrepareUntil=0;
    bool bStarted=false,bWasSprinting=false;
    int32 SecondaryDepth=0;
    FTimerHandle Timer;
    void Tick();
    void SetActive(EAugmentID ID,float Until);
    void CueUse(EAugmentID ID);
    void CueImpact(EAugmentID ID,AActor* Target,float Radius=55);
    float NextAuraRefresh=0;

    bool InternalReady(EAugmentID ID,float Interval);
    TArray<AActor*> Near(const FVector& Location,float Radius,AActor* Ignore=nullptr) const;
    bool Visible(const FVector& From,AActor* Target) const;
    void Secondary(AActor* Target,float Damage);
    void Radial(const FVector& Center,float Radius,float Damage,AActor* Ignore=nullptr);
    void Field(int32 Kind,const FVector& Location,float Radius,float Seconds,float Strength);
    bool SafeMove(const FVector& Destination);
    FVector AimPoint(float Distance) const;
    void ClearBurns();
};
