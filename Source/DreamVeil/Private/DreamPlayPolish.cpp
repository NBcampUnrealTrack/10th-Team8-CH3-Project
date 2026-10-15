#include "DreamPlayPolish.h"
#include "DreamAugmentCatalog.h"
#include "DispatchTableComponent.h"
#include "DreamVeilGameInstance.h"
#include "MainPlayerCharacter.h"
#include "MonsterBase.h"
#include "BossMonster.h"
#include "EliteMonster.h"
#include "DreamObjectPoolSubsystem.h"
#include "DreamAugmentFeedback.h"
#include "DreamAugmentRuntime.h"
#include "AugmentDamageLibrary.h"
#include "MainGameModeBase.h"
#include "MonsterSpawnVolume.h"
#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"
#include "CombatStatsComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "DynamicRHI.h"

static FString ReportText(const FDreamEncounterReport& R)
{
    FString Evolutions;for(auto ID:R.Evolutions)if(const auto* Def=DreamAugmentCatalog::Find(ID)){if(!Evolutions.IsEmpty())Evolutions+=TEXT(" · ");Evolutions+=Def->Name;}
    const int32 Time=FMath::FloorToInt(R.Seconds);
    return FString::Printf(TEXT("%s · %s\n생존 시간 %02d:%02d   처치 %d\n스킬 사용 %d회   받은 피해 %.0f\n주요 피해 원인: %s\n완성한 진화: %s"),*R.Map,*R.Outcome,Time/60,Time%60,R.Kills,R.SkillsUsed,R.DamageTaken,R.MainDamageSource.IsEmpty()?TEXT("없음"):*R.MainDamageSource,Evolutions.IsEmpty()?TEXT("없음"):*Evolutions);
}
void UDreamPlayPreferences::Initialize(FSubsystemCollectionBase& C)
{Super::Initialize(C);GConfig->GetBool(TEXT("DreamVeil.Presentation"),TEXT("SimplifiedEffects"),bSimple,GGameUserSettingsIni);}
void UDreamPlayPreferences::SetSimplifiedEffects(bool Enabled)
{bSimple=Enabled;GConfig->SetBool(TEXT("DreamVeil.Presentation"),TEXT("SimplifiedEffects"),bSimple,GGameUserSettingsIni);GConfig->Flush(false,GGameUserSettingsIni);}
bool UDreamPlayPreferences::TrackEvolution(EAugmentID ID)
{const auto* D=DreamAugmentCatalog::Find(ID);if(!D||!D->Evolution)return false;Goal=ID;return true;}
FString UDreamPlayPreferences::GetGoalText(const UDispatchTableComponent* Table)const
{
    const auto* Def=DreamAugmentCatalog::Find(Goal);if(!Def||!Def->Evolution)return TEXT("진화 목표 없음 · 도감에서 목표를 지정하세요");
    int32 Owned=0;FString Missing;for(auto ID:Def->Ingredients)if(Table&&Table->HasAcquiredAugment(ID))++Owned;else if(const auto* Part=DreamAugmentCatalog::Find(ID)){if(!Missing.IsEmpty())Missing+=TEXT(" · ");Missing+=Part->Name;}
    return FString::Printf(TEXT("목표 · %s %d/%d\n%s"),*Def->Name,Owned,Def->Ingredients.Num(),Table&&Table->HasAcquiredAugment(Goal)?TEXT("진화 완료"):Missing.IsEmpty()?TEXT("재료 완성 · 증강 선택에서 진화 카드 등장 가능"):*(TEXT("부족: ")+Missing));
}
FString UDreamPlayPreferences::GetLastReportText()const{return bHasReport?ReportText(LastReport):TEXT("아직 완료한 전투 기록이 없습니다.");}
float UDreamEncounterDirector::GetWorldTime()const{return GetWorld()?GetWorld()->GetTimeSeconds():0;}
bool UDreamEncounterDirector::IsTickable()const{return !IsTemplate()&&GetWorld()&&GetWorld()->HasBegunPlay();}
float UDreamEncounterDirector::SpawnIntervalMultiplier(int32 Alive,int32 Limit,float HP,float SinceDamage,int32 RecentPacks)
{
    const float Density=float(Alive)/FMath::Max(1,Limit);
    float Mult=Density>=.8f?1.6f:Density>=.55f?1.25f:1.f;
    if(HP<.3f&&SinceDamage<4)Mult=FMath::Max(Mult,1.5f);
    if(RecentPacks>=3)Mult=FMath::Max(Mult,1.35f);
    // Empty maps refill at the original pace; this never makes spawning faster.
    return FMath::Clamp(Mult,1.f,1.8f);
}
float UDreamEncounterDirector::SpawnMultiplier(int32 Alive,int32 Limit)const
{
    auto* P=Cast<AMainPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this,0));const float HP=P&&P->CombatStats?P->CombatStats->GetHealthPercentage():1;
    int32 Recent=0;for(float Time:PackTimes)if(GetWorldTime()-Time<12)++Recent;
    return SpawnIntervalMultiplier(Alive,Limit,HP,GetWorldTime()-LastDamageTime,Recent);
}
bool UDreamEncounterDirector::ReserveAttack(AMonsterBase* M,bool Heavy)
{
    if(!M)return false;const auto* GI=GetWorld()->GetGameInstance<UDreamVeilGameInstance>();if(!GI||(!GI->IsInLevelMap()&&!GI->IsInEndless()))return true;const float Now=GetWorldTime();
    Attacks.RemoveAll([Now](const auto& A){return !A.Monster.IsValid()||A.Monster->IsHidden()||!A.Monster->MonsterCombatStats||A.Monster->MonsterCombatStats->IsDead()||UDreamObjectPoolSubsystem::Generation(A.Monster.Get())!=A.Generation||Now>=A.Until;});
    for(const auto& A:Attacks)if(A.Monster==M)return true;
    int32 Count=0;for(const auto& A:Attacks)if(A.Heavy==Heavy)++Count;
    if(Now<(Heavy?NextHeavy:NextNormal)||Count>=(Heavy?2:3))return false;
    FAttack A;A.Monster=M;A.Generation=UDreamObjectPoolSubsystem::Generation(M);A.Heavy=Heavy;A.Until=Now+8;Attacks.Add(A);
    if(Heavy)NextHeavy=Now+.65f;else NextNormal=Now+.16f;return true;
}
void UDreamEncounterDirector::ReleaseAttack(AMonsterBase* M){Attacks.RemoveAll([M](const auto& A){return A.Monster.Get()==M;});}
void UDreamEncounterDirector::RecordDamage(AActor* Target,AActor* Attacker,float Amount,bool Fire,bool Projectile)
{
    if(!Cast<AMainPlayerCharacter>(Target)||Amount<=0||!bStarted||bFinished)return;
    LastDamageTime=GetWorldTime();Report.DamageTaken+=Amount;
    const FString Key=Cast<ABossMonster>(Attacker)?TEXT("보스 공격"):Cast<AEliteMonster>(Attacker)?TEXT("엘리트 공격"):Fire?TEXT("화염·장판"):Projectile?TEXT("원거리 공격"):TEXT("근접 공격");DamageBySource.FindOrAdd(Key)+=Amount;
}
void UDreamEncounterDirector::Finish(const FString& Outcome)
{
    if(!bStarted||bFinished)return;bFinished=true;Report.Outcome=Outcome;Report.Seconds=GetWorldTime()-Started;
    float Maximum=0;for(const auto& Item:DamageBySource)if(Item.Value>Maximum){Maximum=Item.Value;Report.MainDamageSource=Item.Key;}
    if(auto* P=Cast<AMainPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this,0)))if(P->DispatchTable)for(auto ID:P->DispatchTable->GetAugmentHistory())if(UDispatchTableComponent::IsEvolution(ID))Report.Evolutions.AddUnique(ID);
    if(auto* GI=GetWorld()->GetGameInstance())if(auto* Pref=GI->GetSubsystem<UDreamPlayPreferences>()){Pref->LastReport=Report;Pref->bHasReport=true;}
}
FString UDreamEncounterDirector::CurrentReportText()const{auto Copy=Report;if(!bFinished){Copy.Seconds=GetWorldTime()-Started;Copy.Outcome=TEXT("진행 중");}return ReportText(Copy);}
void UDreamEncounterDirector::Tick(float)
{
    if(!bStarted)if(auto* GI=GetWorld()->GetGameInstance<UDreamVeilGameInstance>())if(GI->IsInLevelMap()||GI->IsInEndless()){bStarted=true;Started=GetWorldTime();Report.Map=UGameplayStatics::GetCurrentLevelName(this,true);}
    if(GetWorldTime()>=NextMaintenance){NextMaintenance=GetWorldTime()+.5f;PackTimes.RemoveAll([this](float Time){return GetWorldTime()-Time>12;});}
    if(CaptureLabel.IsEmpty())return;
    const double Now=FPlatformTime::Seconds();if(LastFrame>0){const float MS=float((Now-LastFrame)*1000);FrameSum+=MS;FrameMax=FMath::Max(FrameMax,MS);GpuSum+=FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());++FrameCount;}LastFrame=Now;
    if(Now-SampleStart<1||FrameCount==0)return;
    int32 Alive=0;for(TActorIterator<AMonsterBase> It(GetWorld());It;++It)if(!It->IsHidden()&&It->MonsterCombatStats&&!It->MonsterCombatStats->IsDead())++Alive;
    const auto Memory=FPlatformMemory::GetStats();auto* Feedback=GetWorld()->GetSubsystem<UDreamAugmentFeedback>();auto* Pool=GetWorld()->GetSubsystem<UDreamObjectPoolSubsystem>();
    const FString Row=FString::Printf(TEXT("%.3f,%.3f,%.3f,%.3f,%.2f,%d,%d,%d,%d"),Now-CaptureStart,FrameSum/FrameCount,FrameMax,GpuSum/FrameCount,double(Memory.UsedPhysical)/1048576,Alive,Feedback?Feedback->GetActiveCueCount():0,Pool?Pool->GetCachedActorCount():0,Pool?Pool->GetActorReuseCount():0);
    CaptureRows.Add(Row);LastPerformanceSummary=Row;FrameSum=GpuSum=FrameMax=0;FrameCount=0;SampleStart=Now;
    if(CaptureRows.Num()%30==0)FFileHelper::SaveStringArrayToFile(CaptureRows,*CaptureCSV);
}
bool UDreamEncounterDirector::StartPerformanceCapture(const FString& Label)
{
    if(Label.IsEmpty()||!CaptureLabel.IsEmpty())return false;CaptureLabel=FPaths::MakeValidFileName(Label);CaptureStart=SampleStart=FPlatformTime::Seconds();LastFrame=0;FrameSum=GpuSum=FrameMax=0;FrameCount=0;
    CaptureCSV=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("Profiling"),CaptureLabel+TEXT("_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))+TEXT(".csv"));IFileManager::Get().MakeDirectory(*FPaths::GetPath(CaptureCSV),true);
    CaptureRows={TEXT("elapsed_s,frame_mean_ms,frame_max_ms,gpu_mean_ms,process_ram_mb,alive_monsters,augment_cues,cached_actors,actor_reuses")};return true;
}
FString UDreamEncounterDirector::StopPerformanceCapture(){if(CaptureLabel.IsEmpty())return FString();FFileHelper::SaveStringArrayToFile(CaptureRows,*CaptureCSV);CaptureLabel.Empty();return CaptureCSV;}
FString UDreamEncounterDirector::GetPerformanceSummary()const{return LastPerformanceSummary;}
void UDreamEncounterDirector::Deinitialize(){StopPerformanceCapture();Super::Deinitialize();}

bool UDreamEncounterDirector::AllowPathRecovery()
{const float Now=GetWorldTime();if(Now<NextPathRecovery)return false;NextPathRecovery=Now+.25f;return true;}
int32 UDreamEncounterDirector::ConfigurePerformanceScenario(int32 MonsterCount)
{
#if !UE_BUILD_SHIPPING
    auto* P=Cast<AMainPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this,0));if(!P)return 0;
    ScenarioTargetCount=FMath::Clamp(MonsterCount,1,32);ScenarioMonsters.Empty();ScenarioStep=0;P->SetCanBeDamaged(false);P->CombatStats->InitStats(10000000,1000000,10);
    if(auto* GM=GetWorld()->GetAuthGameMode<AMainGameModeBase>())GetWorld()->GetTimerManager().ClearAllTimersForObject(GM);
    for(TActorIterator<AMonsterSpawnVolume> It(GetWorld());It;++It)GetWorld()->GetTimerManager().ClearAllTimersForObject(*It);
    for(TActorIterator<AMonsterBase> It(GetWorld());It;++It)It->Destroy();
    if(P->DispatchTable&&P->DispatchTable->GetAugmentHistory().IsEmpty())
        P->DispatchTable->RestoreAugments({EAugmentID::AreaAttack,EAugmentID::ChainRounds,EAugmentID::Resolve,EAugmentID::Knockback,EAugmentID::FocusFire,EAugmentID::AutoShield,EAugmentID::AttackUp,EAugmentID::StaminaUp,EAugmentID::EffectRange,EAugmentID::EffectPower,EAugmentID::DefenceUp,EAugmentID::HealthUp,EAugmentID::SkillExplosion,EAugmentID::SkillChain,EAugmentID::SkillGuard,EAugmentID::SkillOvercharge,EAugmentID::SkillShield,EAugmentID::SuppressionPulse});
    return PerformanceScenarioStep(0);
#else
    return 0;
#endif
}
int32 UDreamEncounterDirector::PerformanceScenarioStep(int32 Mode)
{
#if !UE_BUILD_SHIPPING
    auto* P=Cast<AMainPlayerCharacter>(UGameplayStatics::GetPlayerPawn(this,0));auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!P||!P->DispatchTable||!Nav)return 0;
    P->DispatchTable->bAugmentFeedbackEnabled=Mode!=0;
    auto* Pref=GetWorld()->GetGameInstance()->GetSubsystem<UDreamPlayPreferences>();
    // The benchmark uses an isolated UserDir. Its mode switch never changes saved player progression.
    if(Pref->IsSimplifiedEffects()!=(Mode==2))Pref->SetSimplifiedEffects(Mode==2);
    ScenarioMonsters.RemoveAll([](const auto& M){return !M.IsValid()||M->IsHidden();});
    auto* Class=LoadClass<AMonsterBase>(nullptr,TEXT("/Game/Blueprint/Monsters/BP_MeleeMonster.BP_MeleeMonster_C"));
    for(int32 I=ScenarioMonsters.Num();I<ScenarioTargetCount;++I)
    {
        FNavLocation Point;if(!Nav->GetRandomReachablePointInRadius(P->GetNavAgentLocation(),1800,Point))continue;
        if(FVector::DistSquared2D(Point.Location,P->GetActorLocation())<FMath::Square(400.f))continue;
        auto* M=Cast<AMonsterBase>(UDreamObjectPoolSubsystem::Spawn(GetWorld(),Class,FTransform(Point.Location+FVector(0,0,100))));
        if(M){M->SetCanBeDamaged(false);M->MonsterCombatStats->InitStats(10000000,0,M->MonsterCombatStats->GetAttackPower());ScenarioMonsters.Add(M);}
    }
    auto* Runtime=P->DispatchTable->GetRuntime();
    if(!ScenarioMonsters.IsEmpty())
    {
        auto* Target=ScenarioMonsters[ScenarioStep%ScenarioMonsters.Num()].Get();
        FHitResult Hit(Target,Target->GetCapsuleComponent(),Target->GetActorLocation(),FVector::UpVector);Hit.ImpactPoint=Target->GetActorLocation();Runtime->BeginShot(P->GetActorLocation()+FVector(0,0,40),(Target->GetActorLocation()-P->GetActorLocation()).GetSafeNormal(),10,3000,ECC_Pawn);UAugmentDamageLibrary::ApplyWeaponHit(P,Hit,10);Runtime->EndShot();
        for(auto ID:P->DispatchTable->GetSkillIDs())if(Runtime->CooldownRemaining(ID)<=0)Runtime->Use(ID);
        if(ScenarioStep%5==0){Runtime->BeginShot(P->GetActorLocation()+FVector(0,0,40),(Target->GetActorLocation()-P->GetActorLocation()).GetSafeNormal(),10,3000,ECC_Pawn);Runtime->EndShot();}
        if(ScenarioStep%20==19&&ScenarioMonsters.Num()>4){auto* Old=ScenarioMonsters.Pop().Get();UDreamObjectPoolSubsystem::ReleaseOrDestroy(Old);}
    }
    ++ScenarioStep;return ScenarioMonsters.Num();
#else
    return 0;
#endif
}

UDreamPlayPreferences* UDreamPlayPreferences::Get(const UObject* Context){auto* W=Context?Context->GetWorld():nullptr;return W&&W->GetGameInstance()?W->GetGameInstance()->GetSubsystem<UDreamPlayPreferences>():nullptr;}
UDreamEncounterDirector* UDreamEncounterDirector::Get(const UObject* Context){auto* W=Context?Context->GetWorld():nullptr;return W?W->GetSubsystem<UDreamEncounterDirector>():nullptr;}

