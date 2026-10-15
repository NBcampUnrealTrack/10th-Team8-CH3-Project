#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "AugmentTypes.h"
#include "DreamPlayPolish.generated.h"
class AMonsterBase;
class UDispatchTableComponent;
USTRUCT(BlueprintType)
struct FDreamEncounterReport
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Map;
    UPROPERTY(BlueprintReadOnly) FString Outcome;
    UPROPERTY(BlueprintReadOnly) FString MainDamageSource;
    UPROPERTY(BlueprintReadOnly) float Seconds=0;
    UPROPERTY(BlueprintReadOnly) float DamageTaken=0;
    UPROPERTY(BlueprintReadOnly) int32 Kills=0;
    UPROPERTY(BlueprintReadOnly) int32 SkillsUsed=0;
    UPROPERTY(BlueprintReadOnly) TArray<EAugmentID> Evolutions;
};
UCLASS()
class DREAMVEIL_API UDreamPlayPreferences : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure,Category="Settings",meta=(WorldContext="Context")) static UDreamPlayPreferences* Get(const UObject* Context);
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    UFUNCTION(BlueprintCallable,Category="Settings") void SetSimplifiedEffects(bool Enabled);
    UFUNCTION(BlueprintPure,Category="Settings") bool IsSimplifiedEffects() const { return bSimple; }
    UFUNCTION(BlueprintCallable,Category="Augment") bool TrackEvolution(EAugmentID ID);
    UFUNCTION(BlueprintCallable,Category="Augment") void ClearEvolutionGoal(){Goal=static_cast<EAugmentID>(255);}
    UFUNCTION(BlueprintPure,Category="Augment") EAugmentID GetEvolutionGoal() const { return Goal; }
    UFUNCTION(BlueprintPure,Category="Augment") FString GetGoalText(const UDispatchTableComponent* Table) const;
    UFUNCTION(BlueprintPure,Category="Results") FString GetLastReportText() const;
    FDreamEncounterReport LastReport;
    bool bHasReport=false;
    void ClearRun(){ClearEvolutionGoal();LastReport=FDreamEncounterReport();bHasReport=false;}
private:
    bool bSimple=false;
    EAugmentID Goal=static_cast<EAugmentID>(255);
};
UCLASS()
class DREAMVEIL_API UDreamEncounterDirector : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure,Category="Diagnostics",meta=(WorldContext="Context")) static UDreamEncounterDirector* Get(const UObject* Context);
    virtual void Tick(float DeltaSeconds) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDreamEncounterDirector,STATGROUP_Tickables); }
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override { return Type==EWorldType::Game||Type==EWorldType::PIE; }
    virtual bool IsTickable() const override;
    virtual void Deinitialize() override;
    static float SpawnIntervalMultiplier(int32 Alive,int32 Limit,float HP,float SinceDamage,int32 RecentPacks);
    float SpawnMultiplier(int32 Alive,int32 Limit) const;
    void PackSpawned(){PackTimes.Add(GetWorldTime());}
    bool AllowPathRecovery();
    bool ReserveAttack(AMonsterBase* Monster,bool Heavy);
    void ReleaseAttack(AMonsterBase* Monster);
    void RecordDamage(AActor* Target,AActor* Attacker,float Amount,bool Fire,bool Projectile);
    void RecordKill(){if(!bFinished)++Report.Kills;}
    void RecordSkill(){if(!bFinished)++Report.SkillsUsed;}
    UFUNCTION(BlueprintCallable,Category="Results") void Finish(const FString& Outcome);
    FString CurrentReportText() const;
    float LastDamageTime=-100;
    UFUNCTION(BlueprintCallable,Category="Diagnostics") int32 ConfigurePerformanceScenario(int32 MonsterCount);
    UFUNCTION(BlueprintCallable,Category="Diagnostics") int32 PerformanceScenarioStep(int32 Mode);
    UFUNCTION(BlueprintCallable,Category="Diagnostics") bool StartPerformanceCapture(const FString& Label);
    UFUNCTION(BlueprintCallable,Category="Diagnostics") FString StopPerformanceCapture();
    UFUNCTION(BlueprintPure,Category="Diagnostics") FString GetPerformanceSummary() const;
private:
    float GetWorldTime() const;
    struct FAttack{TWeakObjectPtr<AMonsterBase> Monster;uint32 Generation=0;bool Heavy=false;float Until=0;};
    TArray<FAttack> Attacks;
    TArray<TWeakObjectPtr<AMonsterBase>> ScenarioMonsters;
    int32 ScenarioTargetCount=0,ScenarioStep=0;
    float NextPathRecovery=0,NextMaintenance=0;

    TArray<float> PackTimes;
    float NextHeavy=0,NextNormal=0,Started=0;
    bool bStarted=false,bFinished=false;
    FDreamEncounterReport Report;
    TMap<FString,float> DamageBySource;
    double CaptureStart=0,LastFrame=0,SampleStart=0;
    float FrameSum=0,GpuSum=0,FrameMax=0;
    int32 FrameCount=0;
    FString CaptureLabel,CaptureCSV,LastPerformanceSummary;
    TArray<FString> CaptureRows;
};
