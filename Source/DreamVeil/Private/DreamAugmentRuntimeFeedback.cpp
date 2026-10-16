#include "DreamAugmentRuntime.h"
#include "DreamAugmentFeedback.h"
#include "DispatchTableComponent.h"
#include "MainPlayerCharacter.h"
#include "MainPlayerController.h"
#include "CombatStatsComponent.h"
#include "MonsterBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

bool UDreamAugmentRuntime::HasFeedbackState(EAugmentID ID)const
{
    if(!Stats()||Stats()->IsDead())return false;
    if(ID==static_cast<EAugmentID>(255))return false;
    if(ID==Prepared&&PrepareUntil>Now())return true;
    if(Charges.FindRef(ID)>0)return true;
    if(ID==EAugmentID::AutoShield)return Has(ID)&&Shield>0&&ShieldUntil>Now();
    if(ID==EAugmentID::LastBreath)return Has(ID);
    if(ID==EAugmentID::Berserker)return Has(ID)&&!Has(EAugmentID::LastBreath)&&Stats()->GetHealthPercentage()<=BERSERKER_THRESHOLD;
    if(ID==EAugmentID::LastFortress)return Has(ID)&&!Has(EAugmentID::LastBreath)&&Stats()->GetHealthPercentage()<=LAST_FORTRESS_THRESHOLD;
    return IsActive(ID);
}
void UDreamAugmentRuntime::SetActive(EAugmentID ID,float Until)
{
    const bool WasActive=IsActive(ID);Active.Add(ID,Until);
    if(!WasActive&&static_cast<uint8>(ID)<static_cast<uint8>(EAugmentID::SkillOvercharge))
        UDreamAugmentFeedback::Emit(Player(),ID,EDreamAugmentCueShape::Ring,Player()->GetActorLocation()-FVector(0,0,80),85,.4f);
    UDreamAugmentFeedback::Emit(Player(),ID,EDreamAugmentCueShape::Aura,Player()->GetActorLocation(),72,Until-Now(),Player(),FVector::ZeroVector,true);
}
void UDreamAugmentRuntime::CueImpact(EAugmentID ID,AActor* Target,float Radius)
{
    if(Target)UDreamAugmentFeedback::Emit(Player(),ID,EDreamAugmentCueShape::Impact,Target->GetActorLocation(),FMath::Max(Radius,72.f),.55f);
}
void UDreamAugmentRuntime::CueUse(EAugmentID ID)
{
    auto* P=Player();if(!P)return;const float Range=Dispatch()->GetEffectRangeMultiplier();
    const FVector Ground=P->GetActorLocation()-FVector(0,0,80);
    const auto Ring=[&](const FVector& At,float Radius){UDreamAugmentFeedback::Emit(P,ID,EDreamAugmentCueShape::Ring,At,Radius,.65f);};
    const auto Beam=[&](const FVector& From,const FVector& To){UDreamAugmentFeedback::Emit(P,ID,EDreamAugmentCueShape::Beam,From,3,.25f,nullptr,To);};
    switch(ID)
    {
    case EAugmentID::SkillOvercharge:case EAugmentID::SkillPiercing:case EAugmentID::SkillChain:case EAugmentID::SkillExplosion:case EAugmentID::SkillRift:case EAugmentID::SkillExecute:case EAugmentID::SkillSeal:
        UDreamAugmentFeedback::Emit(P,ID,EDreamAugmentCueShape::Aura,P->GetActorLocation(),65,30,P,FVector::ZeroVector,true);Ring(Ground,85);break;
    case EAugmentID::SuppressionPulse:Ring(Ground,Dispatch()->PulseRadius*Range);break;
    case EAugmentID::SkillTimeLock:Ring(Ground,300*Range);break;
    case EAugmentID::SkillGravity:
        {const FVector Point=AimPoint(500);Ring(Point,300*Range);for(auto* Target:Near(Point,300*Range))if(Visible(Point,Target))Beam(Target->GetActorLocation(),Point+FVector(0,0,35));}break;
    case EAugmentID::SkillScatter:
        {FVector Start;FRotator Rotation;P->GetController()->GetPlayerViewPoint(Start,Rotation);for(int32 I=-2;I<=2;++I){FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(P);const FVector End=Start+(Rotation+FRotator(0,I*3,0)).Vector()*3000;
        const bool Block=GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Q);Beam(P->GetActorLocation()+FVector(0,0,35),Block?Hit.ImpactPoint:End);if(Block&&Cast<AMonsterBase>(Hit.GetActor()))CueImpact(ID,Hit.GetActor());}}break;
    case EAugmentID::SkillFlame:
        {FVector Start;FRotator Rotation;P->GetController()->GetPlayerViewPoint(Start,Rotation);
        for(int32 I=-1;I<=1;++I){const FVector End=Ground+(Rotation+FRotator(0,I*25,0)).Vector().GetSafeNormal2D()*400*Range;Beam(Ground+FVector(0,0,40),End+FVector(0,0,40));}
        }break;
    case EAugmentID::SkillTrap:case EAugmentID::SkillSuppress:case EAugmentID::SkillDecoy:case EAugmentID::SkillBarrier:Ring(Ground,80);break;
    case EAugmentID::SkillReturn:
        UDreamAugmentFeedback::Emit(P,ID,EDreamAugmentCueShape::Field,ReturnPoint-FVector(0,0,80),85,3,nullptr,FVector::ZeroVector,true);Ring(Ground,100);break;
    default:Ring(Ground,95);break;
    }
}
