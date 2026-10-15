#include "DreamAugmentRuntime.h"
#include "DreamObjectPoolSubsystem.h"
#include "MonsterBase.h"
#include "BossMonster.h"
#include "EliteMonster.h"
#include "MonsterSkill.h"
#include "CombatStatsComponent.h"
#include "AugmentDamageLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

UDreamAugmentStatus::UDreamAugmentStatus(){PrimaryComponentTick.bCanEverTick=false;}
UDreamAugmentStatus* UDreamAugmentStatus::For(AActor* Actor)
{
    if(!IsValid(Actor))return nullptr;
    auto* S=Actor->FindComponentByClass<UDreamAugmentStatus>();
    if(!S){S=NewObject<UDreamAugmentStatus>(Actor);Actor->AddInstanceComponent(S);S->RegisterComponent();}return S;
}
void UDreamAugmentStatus::Slow(FName Source,float Amount,float Seconds)
{
    auto* M=Cast<AMonsterBase>(GetOwner());if(!M || !M->MonsterCombatStats || M->MonsterCombatStats->IsDead() || Seconds<=0)return;
    if(M->MonsterSkill && M->MonsterSkill->IsUsingSkill())return;
    auto& S=Slows.FindOrAdd(Source);S.Amount=FMath::Clamp(Amount,0.f,1.f);S.Until=GetWorld()->GetTimeSeconds()+Seconds;
    if(!GetWorld()->GetTimerManager().IsTimerActive(Timer))GetWorld()->GetTimerManager().SetTimer(Timer,this,&UDreamAugmentStatus::Update,.1f,true);
    Update();
}
void UDreamAugmentStatus::Seal(float Seconds)
{
    if(Cast<ABossMonster>(GetOwner()))return;SealUntil=FMath::Max(SealUntil,GetWorld()->GetTimeSeconds()+Seconds);
}
bool UDreamAugmentStatus::IsSealed()const{return GetWorld() && GetWorld()->GetTimeSeconds()<SealUntil;}
void UDreamAugmentStatus::Update()
{
    auto* M=Cast<AMonsterBase>(GetOwner());if(!M || !M->MonsterCombatStats || M->MonsterCombatStats->IsDead()){Reset();return;}
    const float Time=GetWorld()->GetTimeSeconds();float Slowest=0;
    for(auto It=Slows.CreateIterator();It;++It){if(It->Value.Until<=Time)It.RemoveCurrent();else Slowest=FMath::Max(Slowest,It->Value.Amount);}
    auto* Move=M->GetCharacterMovement();
    if(AppliedSpeed<0)BaseSpeed=Move->MaxWalkSpeed;
    else if(!FMath::IsNearlyEqual(Move->MaxWalkSpeed,AppliedSpeed,.1f))BaseSpeed=Move->MaxWalkSpeed;
    AppliedSpeed=BaseSpeed*(1-Slowest);Move->MaxWalkSpeed=AppliedSpeed;
    if(Slowest>=1)Move->StopMovementImmediately();
    if(Slows.IsEmpty()){AppliedSpeed=-1;GetWorld()->GetTimerManager().ClearTimer(Timer);}
}
void UDreamAugmentStatus::Reset()
{
    if(auto* M=Cast<AMonsterBase>(GetOwner()))if(AppliedSpeed>=0 && FMath::IsNearlyEqual(M->GetCharacterMovement()->MaxWalkSpeed,AppliedSpeed,.1f))M->GetCharacterMovement()->MaxWalkSpeed=BaseSpeed;
    Slows.Empty();SealUntil=0;AppliedSpeed=-1;if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(Timer);
}
void UDreamAugmentStatus::EndPlay(const EEndPlayReason::Type Reason){Reset();Super::EndPlay(Reason);}
void UDreamAugmentStatus::PullToward(const FVector& Point,float Distance)
{
    auto* M=Cast<AMonsterBase>(GetOwner());if(!M || Cast<ABossMonster>(M) || M->IsRagdoll() || !M->MonsterCombatStats || M->MonsterCombatStats->IsDead() || (M->MonsterSkill&&M->MonsterSkill->IsUsingSkill()))return;
    FVector Delta=Point-M->GetActorLocation();Delta.Z=0;Delta=Delta.GetClampedToMaxSize(FMath::Clamp(Distance,0.f,300.f));
    FVector End=M->GetActorLocation()+Delta;FHitResult Ground;FCollisionQueryParams Q;Q.AddIgnoredActor(M);
    if(!GetWorld()->LineTraceSingleByChannel(Ground,End+FVector(0,0,100),End-FVector(0,0,250),ECC_Visibility,Q) || Ground.ImpactNormal.Z<.65f)return;
    FHitResult Hit;M->GetCharacterMovement()->SafeMoveUpdatedComponent(Delta,M->GetActorQuat(),true,Hit);
}

static TArray<TWeakObjectPtr<ADreamAugmentField>> ActiveDecoys;
ADreamAugmentField::ADreamAugmentField()
{
    PrimaryActorTick.bCanEverTick=false;AutoPossessAI=EAutoPossessAI::Disabled;
    Sphere=CreateDefaultSubobject<USphereComponent>(TEXT("Area"));SetRootComponent(Sphere);Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Barrier=CreateDefaultSubobject<UBoxComponent>(TEXT("ProjectileBarrier"));Barrier->SetupAttachment(Sphere);Barrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Indicator"));Visual->SetupAttachment(Sphere);Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);Visual->SetCastShadow(false);Visual->SetCanEverAffectNavigation(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cube"));Visual->SetStaticMesh(Mesh.Object);
    Warning=CreateDefaultSubobject<UDecalComponent>(TEXT("GroundIndicator"));Warning->SetupAttachment(Sphere);Warning->SetRelativeRotation(FRotator(-90,0,0));Warning->SetFadeScreenSize(.001f);
}
void ADreamAugmentField::Configure(APawn* OwnerPawn,int32 InKind,float InRadius,float Seconds,float InStrength)
{
    Source=OwnerPawn;SetOwner(OwnerPawn);SetInstigator(OwnerPawn);Kind=InKind;Radius=FMath::Max(1.f,InRadius);Strength=InStrength;Until=GetWorld()->GetTimeSeconds()+Seconds;Health=20;
    Visual->SetVisibility(Kind>=2);Visual->SetRelativeScale3D(Kind==3?FVector(.05f,3.f,2.f):FVector(.25f,.25f,1.f));
    Sphere->SetSphereRadius(Kind==2?30:Radius);Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);Sphere->SetCollisionObjectType(Kind==2?ECC_Pawn:ECC_WorldDynamic);
    Sphere->SetCollisionEnabled(Kind==2?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);Sphere->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Block);
    Barrier->SetBoxExtent(FVector(4,150,100));Barrier->SetCollisionResponseToAllChannels(ECR_Ignore);Barrier->SetCollisionResponseToChannel(ECC_WorldDynamic,ECR_Block);Barrier->SetCollisionEnabled(Kind==3?ECollisionEnabled::QueryAndPhysics:ECollisionEnabled::NoCollision);
    Warning->SetVisibility(Kind<=1);Warning->DecalSize=FVector(50,Radius,Radius);
    if(auto* Monster=Cast<AMonsterBase>(UGameplayStatics::GetActorOfClass(this,AMonsterBase::StaticClass())))Warning->SetDecalMaterial(Monster->GetAttackWarningMaterial());
    if(Kind==2)ActiveDecoys.AddUnique(this);
    GetWorldTimerManager().SetTimer(Timer,this,&ADreamAugmentField::Update,.15f,true);Update();
}
void ADreamAugmentField::Update()
{
    if(!Source.IsValid() || !Source->FindComponentByClass<UCombatStatsComponent>() || Source->FindComponentByClass<UCombatStatsComponent>()->IsDead() || GetWorld()->GetTimeSeconds()>=Until){Expire();return;}
    if(Kind>=2)return;
    TArray<AActor*> Targets,Ignore{this,Source.Get()};UAugmentDamageLibrary::FindTargetsAtLocation(this,GetActorLocation(),Radius,Ignore,Targets);
    bool Trigger=false;for(auto* A:Targets)if(auto* M=Cast<AMonsterBase>(A))
    {
        if(Kind==1){if(!Cast<ABossMonster>(M))UDreamAugmentStatus::For(M)->Slow(TEXT("AugmentTrap"),1.f,Strength);else UDreamAugmentStatus::For(M)->Slow(TEXT("AugmentTrap"),.1f,Strength);Trigger=true;}
        else UDreamAugmentStatus::For(M)->Slow(TEXT("AugmentField"),Cast<ABossMonster>(M)?(Strength<=.15f?.03f:.05f):Strength,.3f);
    }
    if(Trigger)Expire();
}
void ADreamAugmentField::Expire(){UDreamObjectPoolSubsystem::ReleaseOrDestroy(this);}
void ADreamAugmentField::PrepareForPoolReuse(){Health=20;Source.Reset();}
void ADreamAugmentField::OnPoolAcquired(){}
void ADreamAugmentField::OnPoolReturned(){GetWorldTimerManager().ClearTimer(Timer);Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);Barrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);Warning->SetVisibility(false);Visual->SetVisibility(false);Source.Reset();ActiveDecoys.RemoveAll([this](const auto& D){return !D.IsValid()||D.Get()==this;});}
float ADreamAugmentField::TakeDamage(float Damage,const FDamageEvent&,AController*,AActor*){if(Kind!=2)return 0;const float Applied=FMath::Min(Health,FMath::Max(0.f,Damage));Health-=Applied;if(Health<=0)Expire();return Applied;}
APawn* ADreamAugmentField::FindDecoyFor(const AMonsterBase* M,APawn* Fallback)
{
    if(!M || Cast<ABossMonster>(M)||Cast<AEliteMonster>(M))return Fallback;
    ADreamAugmentField* Best=nullptr;float Distance=FMath::Square(1000.f);
    ActiveDecoys.RemoveAll([](const auto& D){return !D.IsValid();});
    for(const auto& D:ActiveDecoys)if(D.IsValid()&&D->GetWorld()==M->GetWorld()&&D->GetWorld()->GetTimeSeconds()<D->Until)
    {const float V=FVector::DistSquared(D->GetActorLocation(),M->GetActorLocation());if(V<Distance){Distance=V;Best=D.Get();}}
    return Best?Best:Fallback;
}
