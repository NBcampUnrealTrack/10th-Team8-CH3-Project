#include "DreamCrowdTacticsSubsystem.h"
#include "MainPlayerCharacter.h"
#include "MonsterBase.h"
#include "EliteMonster.h"
#include "BossMonster.h"
#include "NavigationSystem.h"
#include "Engine/World.h"

bool UDreamCrowdTacticsSubsystem::IsCornered(APawn* Player,FVector& OpenDirection)
{
    if(!IsValid(Player)||!GetWorld())return false;
    const float Now=GetWorld()->GetTimeSeconds();const FVector Position=Player->GetActorLocation();
    if(CachedPlayer!=Player || Player->GetVelocity().SizeSquared2D()>FMath::Square(80.f)
        ||FVector::DistSquared2D(Position,StationaryOrigin)>FMath::Square(100.f))
    {
        CachedPlayer=Player;StationaryOrigin=Position;StationarySince=Now;NextProbe=0;bCorner=false;
    }
    if(Now-StationarySince<2.f)return false;
    if(Now>=NextProbe)
    {
        NextProbe=Now+.75f;bCorner=false;CachedDirection=FVector::ZeroVector;
        TArray<FVector> Normals;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(CrowdCorner),false,Player);
        for(int32 I=0;I<8;++I)
        {
            const float Angle=I*PI/4;FHitResult Hit;
            if(GetWorld()->LineTraceSingleByObjectType(Hit,Position,Position+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*260,
                FCollisionObjectQueryParams(ECC_WorldStatic),Params)&&FMath::Abs(Hit.ImpactNormal.Z)<.4f)
            {
                const FVector Normal=Hit.ImpactNormal.GetSafeNormal2D();
                if(!Normals.ContainsByPredicate([&](const FVector& Other){return FVector::DotProduct(Other,Normal)>.9f;}))Normals.Add(Normal);
            }
        }
        for(int32 I=0;I<Normals.Num();++I)for(int32 J=I+1;J<Normals.Num();++J)
        {
            const float Dot=FVector::DotProduct(Normals[I],Normals[J]);
            // Opposite corridor walls are not a corner.
            if(Dot>-.2f&&Dot<.8f){CachedDirection=(Normals[I]+Normals[J]).GetSafeNormal2D();bCorner=true;}
        }
        for(auto It=Approaches.CreateIterator();It;++It)if(!It.Key().IsValid())It.RemoveCurrent();
    }
    OpenDirection=CachedDirection;return bCorner;
}

bool UDreamCrowdTacticsSubsystem::FindApproach(AMonsterBase* Monster,APawn* Target,FVector& OutLocation)
{
    if(!Monster||!Cast<AMainPlayerCharacter>(Target)||Cast<AEliteMonster>(Monster)||Cast<ABossMonster>(Monster)
        ||Monster->GetAttackType()!=EMonsterAttackType::Melee||Monster->IsAttacking()||Monster->IsRagdoll())return false;
    const float Range=Monster->GetMonsterAttackRange();
    const double Distance=FVector::DistSquared2D(Monster->GetActorLocation(),Target->GetActorLocation());
    if(Distance<=FMath::Square(Range)||Distance>FMath::Square(1600.f))return false;
    FVector Open;if(!IsCornered(Target,Open))return false;
    const float Now=GetWorld()->GetTimeSeconds();
    auto& Cache=Approaches.FindOrAdd(Monster);
    // UObject allocation strides can give an entire pack the same UniqueID % 5.
    // Allocate lanes explicitly, then keep each monster's lane stable while pursuing.
    if(Cache.Lane==INDEX_NONE)Cache.Lane=NextLane++%5;
    if(Cache.Expires>Now&&FVector::DistSquared2D(Cache.PlayerLocation,Target->GetActorLocation())<2500)
    {OutLocation=Cache.Location;return true;}
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());if(!Nav)return false;
    const float Angles[]={-75,-35,0,35,75};
    const FVector Direction=Open.RotateAngleAxis(Angles[Cache.Lane],FVector::UpVector);
    FNavLocation Point;
    if(!Nav->ProjectPointToNavigation(Target->GetNavAgentLocation()+Direction*FMath::Clamp(Range*.75f,100.f,220.f),Point,FVector(80,80,150)))return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(CrowdApproach),false,Target);Params.AddIgnoredActor(Monster);
    if(GetWorld()->LineTraceTestByObjectType(Target->GetActorLocation(),Point.Location+FVector(0,0,80),FCollisionObjectQueryParams(ECC_WorldStatic),Params))return false;
    Cache.PlayerLocation=Target->GetActorLocation();Cache.Location=Point.Location;Cache.Expires=Now+1.f;
    OutLocation=Point.Location;return true;
}
