#include "DreamAugmentRuntime.h"
#include "DreamAugmentFeedback.h"
#include "DreamAugmentCatalog.h"
#include "DispatchTableComponent.h"
#include "MainPlayerCharacter.h"
#include "MainPlayerController.h"
#include "CombatStatsComponent.h"
#include "AugmentDamageLibrary.h"
#include "DreamObjectPoolSubsystem.h"
#include "WeaponBase.h"
#include "MonsterBase.h"
#include "BossMonster.h"
#include "EliteMonster.h"
#include "MonsterSkill.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "Engine/World.h"

UWorld* UDreamAugmentRuntime::GetWorld()const{return GetOuter()?GetOuter()->GetWorld():nullptr;}
UDispatchTableComponent* UDreamAugmentRuntime::Dispatch()const{return Cast<UDispatchTableComponent>(GetOuter());}
AMainPlayerCharacter* UDreamAugmentRuntime::Player()const{return Dispatch()?Cast<AMainPlayerCharacter>(Dispatch()->GetOwner()):nullptr;}
UCombatStatsComponent* UDreamAugmentRuntime::Stats()const{return Player()?Player()->CombatStats.Get():nullptr;}
float UDreamAugmentRuntime::Now()const{return GetWorld()?GetWorld()->GetTimeSeconds():0;}
bool UDreamAugmentRuntime::Has(EAugmentID ID)const{return Dispatch()&&Dispatch()->HasAcquiredAugment(ID);}
float UDreamAugmentRuntime::Scale(EAugmentID ID,float Per,float Cap)const{return Dispatch()?FMath::Min(Cap,Dispatch()->GetStackCount(ID)*Per):0;}
float UDreamAugmentRuntime::Power()const{return 1+Scale(EAugmentID::EffectPower,.04f,1.f);}
float UDreamAugmentRuntime::Duration()const{return 1+Scale(EAugmentID::EffectDuration,.03f,.6f);}
float UDreamAugmentRuntime::Healing()const{return 1+Scale(EAugmentID::HealingEfficiency,.03f,.6f);}
bool UDreamAugmentRuntime::IsActive(EAugmentID ID)const{const auto* End=Active.Find(ID);return End&&*End>Now();}
bool UDreamAugmentRuntime::BlocksFire()const{return IsActive(EAugmentID::SkillRegenerate);}
bool UDreamAugmentRuntime::AllowsSprintFire()const{return IsActive(EAugmentID::SkillMobileFire)&&Player()&&Player()->GetCurrentStamina()>20;}
float UDreamAugmentRuntime::FireRate()const{return 1+(IsActive(EAugmentID::Rampage)?RampageStacks*.03f:0)+(IsActive(EAugmentID::SkillBarrage)?.2f:0);}
float UDreamAugmentRuntime::Recoil()const{return (1-Scale(EAugmentID::RecoilControl,.04f,.6f))*(IsActive(EAugmentID::MobileSupport)?.9f:1)*(IsActive(EAugmentID::SkillBarrage)?1.1f:1);}
float UDreamAugmentRuntime::GetCost(EAugmentID ID)const{const auto* D=DreamAugmentCatalog::Find(ID);return D?FMath::Max(1.f,(ID==EAugmentID::SuppressionPulse?Dispatch()->PulseStaminaCost:D->Cost)*(1-Scale(EAugmentID::SkillEfficiency,.02f,.4f))):0;}
float UDreamAugmentRuntime::GetCooldown(EAugmentID ID)const{const auto* D=DreamAugmentCatalog::Find(ID);return D?FMath::Max(.5f,(ID==EAugmentID::SuppressionPulse?Dispatch()->PulseCooldown:D->Cooldown)*(1-Scale(EAugmentID::CooldownMastery,.02f,.4f))):0;}
float UDreamAugmentRuntime::CooldownRemaining(EAugmentID ID)const{const auto* V=Ready.Find(ID);return V?FMath::Max(0.f,*V-Now()):0;}
bool UDreamAugmentRuntime::IsFireImmune()const{return Now()<FireImmunityUntil;}
float UDreamAugmentRuntime::VampireRatio()const{return VAMPIRE_HEAL_RATIO+(bDirectDamageContext&&IsActive(EAugmentID::SkillVampire)?.0015f:0);}
float UDreamAugmentRuntime::ThornsMultiplier(){if(IsActive(EAugmentID::SkillThorns)&&Now()>=ReflectNext){ReflectNext=Now()+.25f;return 1.5f;}return 1;}
int32 UDreamAugmentRuntime::CombatReward(int32 Base){RewardRemainder+=Base*Scale(EAugmentID::Collector,.03f,.6f);const int32 Extra=FMath::FloorToInt(RewardRemainder);RewardRemainder-=Extra;return Base+Extra;}
FString UDreamAugmentRuntime::SkillState(EAugmentID ID)const
{
    if(!Stats()||Stats()->IsDead())return TEXT("사용 불가");
    if(ID==Prepared&&PrepareUntil>Now())return FString::Printf(TEXT("다음 사격 · %.1f초"),PrepareUntil-Now());
    if(Charges.FindRef(ID)>0)return FString::Printf(TEXT("준비 %d발"),Charges.FindRef(ID));
    if(ID==EAugmentID::SkillReturn&&IsActive(ID))return TEXT("재입력 · 복귀");
    if(IsActive(ID))return FString::Printf(TEXT("효과 %.1f초"),Active.FindRef(ID)-Now());
    if(CooldownRemaining(ID)>0)return FString::Printf(TEXT("%.1f초"),CooldownRemaining(ID));
    if(Player()->GetCurrentStamina()<GetCost(ID)+20)return TEXT("스태미나 부족");return TEXT("준비 완료");
}
void UDreamAugmentRuntime::Start()
{
    if(!Player())return;if(bStarted){Tick();return;}bStarted=true;LastDamage=Now();BaseWalk=Player()->GetAugmentBaseWalkSpeed();BaseSprint=Player()->GetAugmentBaseSprintSpeed();BaseJump=Player()->GetCharacterMovement()->JumpZVelocity;
    GetWorld()->GetTimerManager().SetTimer(Timer,this,&UDreamAugmentRuntime::Tick,.1f,true);Tick();
}
void UDreamAugmentRuntime::Reset()
{
    UDreamAugmentFeedback::StopSource(Player());NextAuraRefresh=0;
    if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(Timer);
    if(Stats()&&FortressBonusApplied!=0)Stats()->AddDefencePower(-FortressBonusApplied);FortressBonusApplied=0;
    ClearBurns();for(auto& P:Fields)if(P.Value.Actor.IsValid()&&UDreamObjectPoolSubsystem::Generation(P.Value.Actor.Get())==P.Value.Generation)P.Value.Actor->Expire();Fields.Empty();
    if(Player()&&bStarted)Player()->ApplyAugmentMovement(BaseWalk,BaseSprint,BaseJump);
    Ready.Empty();Active.Empty();Charges.Empty();Targets.Empty();Prepared=ShotSkill=static_cast<EAugmentID>(255);bStarted=false;Shield=0;
    FocusTarget.Reset();FocusGeneration=FocusCount=RampageStacks=DirectHitCount=ShotNumber=0;
    FocusTime=ShieldUntil=FireImmunityUntil=RewardRemainder=ReflectNext=PrepareUntil=RegenNext=0;
    LastDamage=LastSprintStop=-100;FireSince=LastSprintStart=-1;bWasSprinting=bDirectDamageContext=false;
    ReturnPoint=ShotStart=FVector::ZeroVector;ShotDirection=FVector::ForwardVector;
}
bool UDreamAugmentRuntime::InternalReady(EAugmentID ID,float Interval){if(Ready.FindRef(ID)>Now())return false;Ready.Add(ID,Now()+Interval);return true;}
TArray<AActor*> UDreamAugmentRuntime::Near(const FVector& Point,float Radius,AActor* Ignore)const
{
    TArray<AActor*> Out,Skip{Player()};if(Ignore)Skip.Add(Ignore);UAugmentDamageLibrary::FindTargetsAtLocation(Player(),Point,Radius,Skip,Out);
    Out.RemoveAll([this](AActor* A){auto* S=A?A->FindComponentByClass<UCombatStatsComponent>():nullptr;return !Cast<AMonsterBase>(A)||!S||S->IsDead();});
    Out.Sort([&Point](const AActor& A,const AActor& B){return FVector::DistSquared(A.GetActorLocation(),Point)<FVector::DistSquared(B.GetActorLocation(),Point);});
    if(Out.Num()>64)Out.SetNum(64);return Out;
}
bool UDreamAugmentRuntime::Visible(const FVector& From,AActor* Target)const
{
    FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(Player());
    const bool Block=GetWorld()->LineTraceSingleByChannel(Hit,From,Target->GetActorLocation(),ECC_Visibility,Q);return !Block||Hit.GetActor()==Target;
}
void UDreamAugmentRuntime::Secondary(AActor* Target,float Damage)
{
    if(!Target||SecondaryDepth>=1)return;TGuardValue<int32> Guard(SecondaryDepth,SecondaryDepth+1);
    UGameplayStatics::ApplyDamage(Target,Damage*Power(),Player()->GetController(),Player(),UAugmentSecondaryDamageType::StaticClass());
}
void UDreamAugmentRuntime::Radial(const FVector& Point,float Radius,float Damage,AActor* Ignore)
{
    for(auto* A:Near(Point,Radius,Ignore))if(Visible(Point+FVector(0,0,20),A))Secondary(A,Damage*FMath::Lerp(.3f,1.f,1-FMath::Clamp(FVector::Dist(A->GetActorLocation(),Point)/Radius,0.f,1.f)));
}
void UDreamAugmentRuntime::Field(int32 Kind,const FVector& Point,float Radius,float Seconds,float Strength)
{
    auto& Old=Fields.FindOrAdd(Kind);if(Old.Actor.IsValid()&&UDreamObjectPoolSubsystem::Generation(Old.Actor.Get())==Old.Generation)Old.Actor->Expire();
    auto* A=Cast<ADreamAugmentField>(UDreamObjectPoolSubsystem::Spawn(GetWorld(),ADreamAugmentField::StaticClass(),FTransform(Player()->GetActorRotation(),Point)));
    if(A){A->Configure(Player(),Kind,Radius,Seconds,Strength);Old.Actor=A;Old.Generation=UDreamObjectPoolSubsystem::Generation(A);}
}
FVector UDreamAugmentRuntime::AimPoint(float Distance)const
{
    FVector Start;FRotator Rot;Player()->GetActorEyesViewPoint(Start,Rot);if(Player()->GetController())Player()->GetController()->GetPlayerViewPoint(Start,Rot);
    FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(Player());FVector End=Start+Rot.Vector()*Distance;
    if(GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Q))End=Hit.ImpactPoint;
    if(GetWorld()->LineTraceSingleByChannel(Hit,End+FVector(0,0,100),End-FVector(0,0,600),ECC_Visibility,Q))End=Hit.ImpactPoint+FVector(0,0,8);return End;
}
bool UDreamAugmentRuntime::SafeMove(const FVector& Destination)
{
    auto* P=Player();if(!P||P->GetCharacterMovement()->IsFalling())return false;
    const FVector Start=P->GetActorLocation();const FVector Delta=Destination-Start;
    if(Delta.Size()>510)return false;
    FCollisionQueryParams Q;Q.AddIgnoredActor(P);FHitResult Ground;
    for(int32 I=1;I<=FMath::Max(1,FMath::CeilToInt(Delta.Size()/50));++I)
    {
        const FVector At=Start+Delta*FMath::Min(1.f,I*50.f/FMath::Max(1.f,Delta.Size()));
        if(!GetWorld()->LineTraceSingleByChannel(Ground,At,At-FVector(0,0,P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+100),ECC_Visibility,Q)||Ground.ImpactNormal.Z<.65f)return false;
    }
    FHitResult Hit;P->GetCharacterMovement()->SafeMoveUpdatedComponent(Delta,P->GetActorQuat(),true,Hit);return FVector::DistSquared(Start,P->GetActorLocation())>100;
}
void UDreamAugmentRuntime::UnlockSkill(EAugmentID ID){Ready.Add(ID,Now()+GetCooldown(ID));}
bool UDreamAugmentRuntime::Use(EAugmentID ID)
{
    auto* P=Player();auto* PC=P?Cast<AMainPlayerController>(P->GetController()):nullptr;const auto* Def=DreamAugmentCatalog::Find(ID);
    if(!PC||!Def||!Def->Evolution||Def->Permanent||!Has(ID)||!Stats()||Stats()->IsDead()||UGameplayStatics::IsGamePaused(this)||PC->IsMenuWidgetOpen()||P->HasAugmentChoices()||PC->IsMoveInputIgnored())return false;
    if(ID==EAugmentID::SkillReturn&&IsActive(ID)){if(FVector::Dist(P->GetActorLocation(),ReturnPoint)>500||!SafeMove(ReturnPoint))return false;Active.Remove(ID);UDreamAugmentFeedback::Emit(P,ID,EDreamAugmentCueShape::Ring,P->GetActorLocation()-FVector(0,0,80),100,.5f);return true;}
    if(Prepared==ID&&PrepareUntil>Now()){Prepared=static_cast<EAugmentID>(255);return true;}
    if(CooldownRemaining(ID)>0||IsActive(ID)||Charges.FindRef(ID)>0||P->GetCurrentStamina()<GetCost(ID)+20)return false;
    const bool PreparedShot=ID==EAugmentID::SkillOvercharge||ID==EAugmentID::SkillPiercing||ID==EAugmentID::SkillChain||ID==EAugmentID::SkillExplosion||ID==EAugmentID::SkillRift||ID==EAugmentID::SkillExecute||ID==EAugmentID::SkillSeal;
    if(PreparedShot){if(PrepareUntil>Now())return false;for(auto Pair:Charges)if(Pair.Value>0)return false;}
    if(ID==EAugmentID::SkillOvercharge){Prepared=ID;PrepareUntil=Now()+3;CueUse(ID);return true;}
    const FVector BeforeMove=P->GetActorLocation();
    if(ID==EAugmentID::SkillDodge){FVector Dir=P->GetLastMovementInputVector().GetSafeNormal2D();if(Dir.IsNearlyZero())Dir=P->GetActorForwardVector();if(!SafeMove(P->GetActorLocation()+Dir*200))return false;}
    if(ID==EAugmentID::SkillReturn&&P->GetCharacterMovement()->IsFalling())return false;
    if(!P->TrySpendSkillStamina(GetCost(ID)))return false;Ready.Add(ID,Now()+GetCooldown(ID));
    const float Time=Now(),Dur=Duration(),Range=Dispatch()->GetEffectRangeMultiplier();const float Damage=P->GetCurrentWeapon()?P->GetCurrentWeapon()->GetFinalDamage():Stats()->GetAttackPower();
    switch(ID)
    {
    case EAugmentID::SkillPiercing:case EAugmentID::SkillExplosion:case EAugmentID::SkillRift:case EAugmentID::SkillExecute:case EAugmentID::SkillSeal:Charges.Add(ID,1);break;
    case EAugmentID::SkillChain:Charges.Add(ID,3);break;
    case EAugmentID::SkillScatter:
        {FVector Start;FRotator Rotation;PC->GetPlayerViewPoint(Start,Rotation);for(int32 I=-2;I<=2;++I){FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(P);if(GetWorld()->LineTraceSingleByChannel(Hit,Start,Start+(Rotation+FRotator(0,I*3,0)).Vector()*3000,ECC_Visibility,Q)&&Cast<AMonsterBase>(Hit.GetActor()))Secondary(Hit.GetActor(),Damage*.3f);}}break;
    case EAugmentID::SkillFlame:
        {FVector Start;FRotator Rotation;PC->GetPlayerViewPoint(Start,Rotation);for(auto* A:Near(P->GetActorLocation(),400*Range))if(FVector::DotProduct((A->GetActorLocation()-P->GetActorLocation()).GetSafeNormal2D(),Rotation.Vector().GetSafeNormal2D())>.8f&&Visible(Start,A)){Secondary(A,Damage*.4f);Burn(A,Damage,3*Dur);}}break;
    case EAugmentID::SkillChill:SetActive(ID,Time+4*Dur);break;
    case EAugmentID::SkillBarrage:SetActive(ID,Time+3*Dur);break;
    case EAugmentID::SuppressionPulse:
        for(auto* A:Near(P->GetActorLocation(),Dispatch()->PulseRadius*Range))if(Visible(P->GetActorLocation(),A))CastChecked<AMonsterBase>(A)->TryApplyAugmentStagger(Dispatch()->PulseStaggerDuration);break;
    case EAugmentID::SkillGravity:
        {const FVector Point=AimPoint(500);for(auto* A:Near(Point,300*Range))if(Visible(Point,A))UDreamAugmentStatus::For(A)->PullToward(Point,300);}break;
    case EAugmentID::SkillTrap:Field(1,P->GetActorLocation()-FVector(0,0,80),200*Range,8*Dur,.6f);break;
    case EAugmentID::SkillSuppress:Field(0,AimPoint(500),250*Range,4*Dur,.25f);break;
    case EAugmentID::SkillDecoy:Field(2,AimPoint(300)+FVector(0,0,50),300,3*Dur,0);break;
    case EAugmentID::SkillBarrier:Field(3,P->GetActorLocation()+P->GetActorForwardVector()*120,150,3*Dur,0);break;
    case EAugmentID::SkillTimeLock:
        for(auto* A:Near(P->GetActorLocation(),300*Range))if(Visible(P->GetActorLocation(),A)){if(Cast<AEliteMonster>(A))UDreamAugmentStatus::For(A)->Slow(TEXT("TimeLock"),.25f,.6f);else if(!Cast<ABossMonster>(A))CastChecked<AMonsterBase>(A)->TryApplyAugmentStagger(.6f,3);}break;
    case EAugmentID::SkillGuard:SetActive(ID,Time+1*Dur);break;
    case EAugmentID::SkillShield:Shield=FMath::Max(Shield,Stats()->GetMaxHealth()*.1f);ShieldUntil=Time+3*Dur;SetActive(ID,ShieldUntil);break;
    case EAugmentID::SkillCounter:SetActive(ID,Time+.4f);break;
    case EAugmentID::SkillThorns:SetActive(ID,Time+3*Dur);break;
    case EAugmentID::SkillVampire:SetActive(ID,Time+4*Dur);break;
    case EAugmentID::SkillRegenerate:SetActive(ID,Time+2);RegenNext=Time+.1f;break;
    case EAugmentID::SkillCleanse:Stats()->SetOnFire(false,nullptr);FireImmunityUntil=Time+1;break;
    case EAugmentID::SkillFortress:SetActive(ID,Time+4*Dur);break;
    case EAugmentID::SkillSprint:SetActive(ID,Time+2*Dur);break;
    case EAugmentID::SkillReturn:ReturnPoint=P->GetActorLocation();SetActive(ID,Time+3);break;
    case EAugmentID::SkillMobileFire:SetActive(ID,Time+3*Dur);break;
    case EAugmentID::SkillDodge:break;
    default:return false;
    }
    if(ID==EAugmentID::SkillDodge)UDreamAugmentFeedback::Emit(P,ID,EDreamAugmentCueShape::Beam,BeforeMove-FVector(0,0,50),3,.3f,nullptr,P->GetActorLocation()-FVector(0,0,50));
    CueUse(ID);Tick();return true;
}
bool UDreamAugmentRuntime::BeginShot(const FVector& Start,const FVector& Direction,float Damage,float Range,ECollisionChannel Channel)
{
    ++ShotNumber;ShotSkill=static_cast<EAugmentID>(255);ShotStart=Start;ShotDirection=Direction;
    if(Prepared==EAugmentID::SkillOvercharge&&PrepareUntil>=Now())
    {if(Player()->TrySpendSkillStamina(GetCost(Prepared))){ShotSkill=Prepared;Ready.Add(Prepared,Now()+GetCooldown(Prepared));}Prepared=static_cast<EAugmentID>(255);}
    else for(auto ID:Dispatch()->GetSkillIDs())if(Charges.FindRef(ID)>0){ShotSkill=ID;Charges[ID]--;break;}
    const bool Piercing=ShotSkill==EAugmentID::SkillPiercing || (Has(EAugmentID::PiercingRounds)&&ShotNumber%4==0);
    if(ShotSkill!=static_cast<EAugmentID>(255)||Piercing)
    {
        FHitResult CueHit;FCollisionQueryParams CueQuery;CueQuery.AddIgnoredActor(Player());
        const FVector BeamEnd=GetWorld()->LineTraceSingleByChannel(CueHit,Start,Start+Direction*Range,Channel,CueQuery)?CueHit.ImpactPoint:Start+Direction*FMath::Min(Range,2000.f);
        UDreamAugmentFeedback::Emit(Player(),ShotSkill==static_cast<EAugmentID>(255)?EAugmentID::PiercingRounds:ShotSkill,EDreamAugmentCueShape::Beam,Player()->GetActorLocation()+FVector(0,0,35),3,.2f,nullptr,BeamEnd);
    }
    if(!Piercing)return false;
    FCollisionQueryParams Q;Q.AddIgnoredActor(Player());const FVector End=Start+Direction*Range;
    for(int32 I=0;I<3;++I)
    {
        FHitResult Hit;if(!GetWorld()->LineTraceSingleByChannel(Hit,Start,End,Channel,Q))break;auto* M=Cast<AMonsterBase>(Hit.GetActor());if(!M)break;
        const float Ratio=I==0?1:ShotSkill==EAugmentID::SkillPiercing?(I==1?.6f:.4f):(I==1?.5f:.25f);
        if(I==0){const float Applied=UAugmentDamageLibrary::ApplyWeaponHit(Player(),Hit,Damage);if(Applied>0&&Player()->GetCurrentWeapon())Player()->GetCurrentWeapon()->OnHitConfirmed.Broadcast(M->MonsterCombatStats->IsDead(),M->IsHeadshotHit(Hit));}
        else Secondary(M,Damage*Ratio);CueImpact(ShotSkill==EAugmentID::SkillPiercing?ShotSkill:EAugmentID::PiercingRounds,M);Q.AddIgnoredActor(M);
    }
    return true;
}
void UDreamAugmentRuntime::EndShot(){ShotSkill=static_cast<EAugmentID>(255);}
float UDreamAugmentRuntime::ModifyDirectDamage(AActor* Target,float Damage,bool Headshot)
{
    if(!Target)return Damage;auto* S=Target->FindComponentByClass<UCombatStatsComponent>();if(!S||S->IsDead())return Damage;
    const bool Boss=Cast<ABossMonster>(Target)!=nullptr;float Bonus=0;
    if(Headshot)Damage*= (HEADSHOT_DAMAGE_MULTIPLIER+Scale(EAugmentID::Weakpoint,.05f,1.f))/HEADSHOT_DAMAGE_MULTIPLIER;
    if(Boss)Bonus+=Scale(EAugmentID::BossHunter,.03f,.6f);else if(Cast<AEliteMonster>(Target))Bonus+=Scale(EAugmentID::EliteHunter,.03f,.6f);
    if(Has(EAugmentID::FocusFire)&&FocusTarget==Target&&FocusGeneration==UDreamObjectPoolSubsystem::Generation(Target)&&Now()-FocusTime<=2&&FocusCount==3)Bonus+=.25f;
    auto* Track=Targets.Find(Target);if(Track&&Track->Generation==UDreamObjectPoolSubsystem::Generation(Target)&&Track->MarkUntil>Now())Bonus+=Track->EmpoweredMark?(Boss?.05f:.15f):(Boss?.02f:.05f);
    if(Has(EAugmentID::ExecutionRounds)&&S->GetHealthPercentage()<=.25f)Bonus+=Boss?.05f:.15f;
    if(IsActive(EAugmentID::Riposte)){Bonus+=.15f;Active.Remove(EAugmentID::Riposte);}
    if(IsActive(EAugmentID::SkillCounter)&&Charges.FindRef(EAugmentID::SkillCounter)>0){Bonus+=.2f;Charges.Remove(EAugmentID::SkillCounter);Active.Remove(EAugmentID::SkillCounter);}
    if(ShotSkill==EAugmentID::SkillOvercharge)Bonus+=.35f;
    if(ShotSkill==EAugmentID::SkillExecute)Bonus+=S->GetHealthPercentage()<=.25f?(Boss?.15f:.5f):.1f;
    Damage*=1+Bonus;const float Defence=S->GetDefencePower(),Pierce=Scale(EAugmentID::ArmorPierce,1,30);
    Damage*= (DEFENCE_REDUCTION_CONSTANT+Defence)/(DEFENCE_REDUCTION_CONSTANT+FMath::Max(0.f,Defence-Pierce));return Damage;
}
void UDreamAugmentRuntime::AfterDirectHit(AActor* Target,float Damage,bool Killed,const FHitResult& Hit)
{
    if(!Target||!Cast<AMonsterBase>(Target)||SecondaryDepth>0)return;++DirectHitCount;
    const int32 Gen=UDreamObjectPoolSubsystem::Generation(Target);
    if(FocusTarget!=Target||FocusGeneration!=Gen||Now()-FocusTime>2)FocusCount=0;FocusTarget=Target;FocusGeneration=Gen;FocusTime=Now();FocusCount=(FocusCount+1)%4;
    auto& Track=Targets.FindOrAdd(Target);if(Track.Generation!=Gen)Track=FTargetState();Track.Generation=Gen;Track.Last=Now();++Track.Hits;
    const bool Boss=Cast<ABossMonster>(Target)!=nullptr;const float Dur=Duration(),Range=Dispatch()->GetEffectRangeMultiplier();
    if(ShotSkill==EAugmentID::SkillRift || (Has(EAugmentID::RiftRounds)&&Track.Hits%3==0)) {const bool Strong=ShotSkill==EAugmentID::SkillRift || (Track.EmpoweredMark&&Track.MarkUntil>Now());Track.MarkUntil=Now()+3*Dur;Track.EmpoweredMark=Strong;UDreamAugmentFeedback::Emit(Player(),Strong?EAugmentID::SkillRift:EAugmentID::RiftRounds,EDreamAugmentCueShape::Mark,Target->GetActorLocation(),38,3*Dur,Target);}
    if(!Killed)
    {
        if(Has(EAugmentID::ChillingRounds)||IsActive(EAugmentID::SkillChill))
        {if(UDreamAugmentStatus::For(Target)->Slow(TEXT("Chill"),IsActive(EAugmentID::SkillChill)?(Boss?.05f:.2f):(Boss?.03f:.1f),2*Dur))UDreamAugmentFeedback::Emit(Player(),EAugmentID::ChillingRounds,EDreamAugmentCueShape::Mark,Target->GetActorLocation(),32,2*Dur,Target);}
        if(Has(EAugmentID::GravityMark)&&Track.Hits%5==0&&InternalReady(EAugmentID::GravityMark,4)){for(auto* Other:Near(Target->GetActorLocation(),200*Range,Target))if(!Cast<ABossMonster>(Other)&&Visible(Target->GetActorLocation(),Other)){UDreamAugmentStatus::For(Other)->PullToward(Target->GetActorLocation(),100);UDreamAugmentFeedback::Emit(Player(),EAugmentID::GravityMark,EDreamAugmentCueShape::Beam,Other->GetActorLocation(),3,.3f,nullptr,Target->GetActorLocation());break;}}
        if(Has(EAugmentID::StopMark)&&Track.Hits%8==0&&CastChecked<AMonsterBase>(Target)->TryApplyAugmentStagger(.2f,3))CueImpact(EAugmentID::StopMark,Target);
        if(!Boss && (ShotSkill==EAugmentID::SkillSeal||(Has(EAugmentID::SealRounds)&&Track.Hits%5==0&&InternalReady(EAugmentID::SealRounds,6)))){const float Seconds=(ShotSkill==EAugmentID::SkillSeal?2:.5f)*Dur;UDreamAugmentStatus::For(Target)->Seal(Seconds);UDreamAugmentFeedback::Emit(Player(),EAugmentID::SealRounds,EDreamAugmentCueShape::Mark,Target->GetActorLocation(),42,Seconds,Target);}
    }
    if(Has(EAugmentID::FragmentRounds)&&DirectHitCount%5==0){int32 Count=0;for(auto* A:Near(Hit.ImpactPoint,200*Range,Target))if(Visible(Hit.ImpactPoint,A)){Secondary(A,Damage*.15f);UDreamAugmentFeedback::Emit(Player(),EAugmentID::FragmentRounds,EDreamAugmentCueShape::Beam,Hit.ImpactPoint,3,.18f,nullptr,A->GetActorLocation());if(++Count==2)break;}}
    if(ShotSkill==EAugmentID::SkillChain||(Has(EAugmentID::ChainRounds)&&DirectHitCount%5==0&&InternalReady(EAugmentID::ChainRounds,1)))
        for(auto* A:Near(Hit.ImpactPoint,300*Range,Target))if(Visible(Hit.ImpactPoint,A)){UDreamAugmentFeedback::Emit(Player(),ShotSkill==EAugmentID::SkillChain?ShotSkill:EAugmentID::ChainRounds,EDreamAugmentCueShape::Beam,Hit.ImpactPoint,3,.25f,nullptr,A->GetActorLocation());Secondary(A,Damage*.3f);break;}
    if(ShotSkill==EAugmentID::SkillExplosion){UDreamAugmentFeedback::Emit(Player(),ShotSkill,EDreamAugmentCueShape::Ring,Hit.ImpactPoint+FVector(0,0,8),300*Range,.65f);UDreamAugmentFeedback::Emit(Player(),ShotSkill,EDreamAugmentCueShape::Impact,Hit.ImpactPoint,95,.45f);Radial(Hit.ImpactPoint,300*Range,Damage,Target);}
    else if(ShotSkill==EAugmentID::SkillExecute||ShotSkill==EAugmentID::SkillOvercharge)CueImpact(ShotSkill,Target,100);
    if(Has(EAugmentID::FocusFire)&&FocusCount==0)CueImpact(EAugmentID::FocusFire,Target);
    if(Has(EAugmentID::ExecutionRounds)){auto* S=Target->FindComponentByClass<UCombatStatsComponent>();if(S&&S->GetHealthPercentage()<=.25f)CueImpact(EAugmentID::ExecutionRounds,Target);}
    if(Has(EAugmentID::RetreatTactics)&&FVector::DotProduct(Player()->GetVelocity().GetSafeNormal2D(),Player()->GetActorForwardVector())<-.25f&&InternalReady(EAugmentID::RetreatTactics,3))SetActive(EAugmentID::RetreatTactics,Now()+1*Dur);
    if(Killed)
    {
        if(Has(EAugmentID::Rampage)){RampageStacks=FMath::Min(5,IsActive(EAugmentID::Rampage)?RampageStacks+1:1);SetActive(EAugmentID::Rampage,Now()+3*Dur);}
        if(Has(EAugmentID::KillTrap)&&InternalReady(EAugmentID::KillTrap,8))Field(1,Hit.ImpactPoint,200*Range,5*Dur,.3f);
        if(Has(EAugmentID::SuppressionEcho)&&InternalReady(EAugmentID::SuppressionEcho,5))Field(0,Hit.ImpactPoint,200*Range,3*Dur,.15f);
    }
}
void UDreamAugmentRuntime::AfterHealthDamage(AActor* Attacker,float Actual,bool Fire)
{
    if(Actual<=0)return;LastDamage=Now();if(Fire&&FireSince<0)FireSince=Now();
    if(IsActive(EAugmentID::SkillRegenerate))Active.Remove(EAugmentID::SkillRegenerate);
    if(Has(EAugmentID::Riposte)&&InternalReady(EAugmentID::Riposte,6))SetActive(EAugmentID::Riposte,Now()+2);
    if(Has(EAugmentID::CrisisEscape)&&InternalReady(EAugmentID::CrisisEscape,8))SetActive(EAugmentID::CrisisEscape,Now()+1.5f*Duration());
    if(Has(EAugmentID::Afterimage)&&Attacker&&InternalReady(EAugmentID::Afterimage,8))Field(2,Attacker->GetActorLocation()+Attacker->GetActorRightVector()*100,300,1,0);
}
float UDreamAugmentRuntime::AbsorbDamage(float Damage,bool Projectile,bool Fire)
{
    if(Damage<=0)return 0;if(Fire&&IsFireImmune())return 0;
    float Reduce=0;
    if(Projectile&&Has(EAugmentID::ProjectileGuard)&&InternalReady(EAugmentID::ProjectileGuard,3))Reduce=.1f;
    if(Has(EAugmentID::Resolve)&&Now()-LastDamage>=4&&InternalReady(EAugmentID::Resolve,12))Reduce=FMath::Max(Reduce,.2f);
    if(IsActive(EAugmentID::SkillGuard)){Reduce=FMath::Max(Reduce,.3f);Active.Remove(EAugmentID::SkillGuard);}
    if(IsActive(EAugmentID::SkillCounter)&&Charges.FindRef(EAugmentID::SkillCounter)==0){Reduce=.5f;Charges.Add(EAugmentID::SkillCounter,1);SetActive(EAugmentID::SkillCounter,Now()+2);}
    if(Reduce>0)UDreamAugmentFeedback::Emit(Player(),IsActive(EAugmentID::SkillCounter)?EAugmentID::SkillCounter:EAugmentID::ProjectileGuard,EDreamAugmentCueShape::Impact,Player()->GetActorLocation(),85,.35f);
    Damage=FMath::Max(MIN_DAMAGE,Damage*(1-Reduce));
    if(ShieldUntil>Now()&&Shield>0){const float Used=FMath::Min(Shield,Damage);Shield-=Used;Damage-=Used;if(Used>0)UDreamAugmentFeedback::Emit(Player(),EAugmentID::SkillShield,EDreamAugmentCueShape::Impact,Player()->GetActorLocation(),75,.3f);}return Damage;
}
void UDreamAugmentRuntime::Burn(AActor* Target,float BaseDamage,float Seconds)
{
    if(!Target)return;UDreamAugmentFeedback::Emit(Player(),EAugmentID::ContinuousAttack,EDreamAugmentCueShape::Mark,Target->GetActorLocation(),30,Seconds,Target);const int32 Gen=UDreamObjectPoolSubsystem::Generation(Target);auto* Existing=Burns.FindByPredicate([Target,Gen](const FBurn& B){return B.Target==Target&&B.Generation==Gen;});
    if(Existing){Existing->Damage=BaseDamage*.1f*Power();Existing->Until=Now()+Seconds;return;}
    FBurn B;B.Target=Target;B.Generation=Gen;B.Damage=BaseDamage*.1f*Power();B.Next=Now()+1;B.Until=Now()+Seconds;Burns.Add(B);
    if(auto* S=Target->FindComponentByClass<UCombatStatsComponent>())S->SetOnFire(true,Dispatch()->OnFireEffect);
}
void UDreamAugmentRuntime::ClearBurns(){for(const auto& B:Burns)if(B.Target.IsValid()&&B.Generation==UDreamObjectPoolSubsystem::Generation(B.Target.Get()))if(auto* S=B.Target->FindComponentByClass<UCombatStatsComponent>())S->SetOnFire(false,nullptr);Burns.Empty();}
void UDreamAugmentRuntime::Tick()
{
    if(!Player()||!Stats()||Stats()->IsDead()){Reset();return;}const float Time=Now();
    if(Time>=NextAuraRefresh)
    {
        NextAuraRefresh=Time+.75f;
        for(auto ID:{EAugmentID::Berserker,EAugmentID::LastFortress,EAugmentID::LastBreath,EAugmentID::AutoShield})
            if(HasFeedbackState(ID))UDreamAugmentFeedback::Emit(Player(),ID,EDreamAugmentCueShape::Aura,Player()->GetActorLocation(),ID==EAugmentID::LastBreath?88:70,1.f,Player(),FVector::ZeroVector,true);
    }
    for(auto ID:Dispatch()->GetSkillIDs())
        if((Prepared==ID||Charges.FindRef(ID)>0)&&HasFeedbackState(ID))UDreamAugmentFeedback::Emit(Player(),ID,EDreamAugmentCueShape::Aura,Player()->GetActorLocation(),65,1,Player(),FVector::ZeroVector,true);
    if(ShieldUntil<=Time)Shield=0;
    if(Shield<=0)Active.Remove(EAugmentID::SkillShield);
    if(!IsActive(EAugmentID::SkillCounter))Charges.Remove(EAugmentID::SkillCounter);
    if(PrepareUntil<=Time)Prepared=static_cast<EAugmentID>(255);
    if(Has(EAugmentID::AutoShield)&&Time-LastDamage>=6&&Shield<=0&&InternalReady(EAugmentID::AutoShield,12)){Shield=Stats()->GetMaxHealth()*.05f;ShieldUntil=Time+3*Duration();UDreamAugmentFeedback::Emit(Player(),EAugmentID::AutoShield,EDreamAugmentCueShape::Ring,Player()->GetActorLocation()-FVector(0,0,80),85,.45f);}
    if(Stats()->IsOnFire())
    {
        if(FireSince<0)FireSince=Time;
        if(Has(EAugmentID::AutoCleanse)&&Time-FireSince>=2&&InternalReady(EAugmentID::AutoCleanse,12)){Stats()->SetOnFire(false,nullptr);FireSince=-1;UDreamAugmentFeedback::Emit(Player(),EAugmentID::AutoCleanse,EDreamAugmentCueShape::Ring,Player()->GetActorLocation()-FVector(0,0,80),85,.5f);}
    }else FireSince=-1;
    const bool Sprinting=Player()->IsActuallySprinting();
    if(Sprinting&&!bWasSprinting)LastSprintStart=Time;
    if(!Sprinting&&bWasSprinting){LastSprintStop=Time;if(Has(EAugmentID::MobileSupport)&&Time-LastSprintStart>=.5f&&InternalReady(EAugmentID::MobileSupport,4))SetActive(EAugmentID::MobileSupport,Time+2*Duration());}
    if(Sprinting&&Time-LastSprintStart>=.5f&&Has(EAugmentID::SprintBoost)&&InternalReady(EAugmentID::SprintBoost,6))SetActive(EAugmentID::SprintBoost,Time+1*Duration());
    bWasSprinting=Sprinting;
    if(IsActive(EAugmentID::SkillMobileFire)&&Player()->GetCurrentStamina()<=20)Active.Remove(EAugmentID::SkillMobileFire);
    float Walk=1+Scale(EAugmentID::WalkSpeedUp,.02f,.3f),Sprint=1+Scale(EAugmentID::SprintSpeedUp,.02f,.3f);
    float MoveBonus=IsActive(EAugmentID::CrisisEscape)?.1f:IsActive(EAugmentID::RetreatTactics)?.05f:0;
    Walk*=1+MoveBonus;Sprint*=1+(IsActive(EAugmentID::SkillSprint)?.2f:IsActive(EAugmentID::SprintBoost)?.05f:0);
    if(IsActive(EAugmentID::SkillMobileFire))Sprint*=.8f;
    if(IsActive(EAugmentID::SkillRegenerate))Walk*=.7f;
    if(IsActive(EAugmentID::SkillFortress))Walk*=.9f;
    Player()->ApplyAugmentMovement(BaseWalk*Walk,BaseSprint*Sprint,BaseJump*(1+Scale(EAugmentID::JumpUp,.03f,.3f)));
    const float DesiredBonus=IsActive(EAugmentID::SkillFortress)&&!Has(EAugmentID::LastBreath)?5:0;
    if(DesiredBonus!=FortressBonusApplied){Stats()->AddDefencePower(DesiredBonus-FortressBonusApplied);FortressBonusApplied=DesiredBonus;}
    if(IsActive(EAugmentID::SkillRegenerate)&&Time>=RegenNext){RegenNext+=.1f;Stats()->Heal(Stats()->GetMaxHealth()*.002f);}
    for(auto It=Targets.CreateIterator();It;++It)if(!It.Key().IsValid()||It.Value().Generation!=UDreamObjectPoolSubsystem::Generation(It.Key().Get())||(Time-It.Value().Last>10&&It.Value().MarkUntil<Time))It.RemoveCurrent();
    for(int32 I=Burns.Num()-1;I>=0;--I)
    {
        FBurn B=Burns[I];auto* Target=B.Target.Get();auto* S=Target?Target->FindComponentByClass<UCombatStatsComponent>():nullptr;
        const bool Valid=S&&B.Generation==UDreamObjectPoolSubsystem::Generation(Target)&&!S->IsDead()&&S->IsOnFire();
        if(!Valid){Burns.RemoveAtSwap(I);continue;}
        if(Time>=B.Next&&B.Next<=B.Until+.02f){Burns[I].Next+=1;UAugmentDamageLibrary::ApplyFireDamage(Player(),Target,B.Damage);}
        if(Time>B.Until+.02f&&Burns[I].Next>B.Until+.02f){S->SetOnFire(false,nullptr);Burns.RemoveAtSwap(I);}
    }
}
