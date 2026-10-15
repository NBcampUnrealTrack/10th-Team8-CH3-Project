#include "DreamAugmentFeedback.h"
#include "DreamAugmentRuntime.h"
#include "DreamAugmentCatalog.h"
#include "DreamObjectPoolSubsystem.h"
#include "DispatchTableComponent.h"
#include "MainPlayerCharacter.h"
#include "MainPlayerController.h"
#include "CombatStatsComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

UDreamAugmentFeedback::UDreamAugmentFeedback()
{
    static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> Ring(TEXT("/Game/Effects/Augments/Feedback/M_AugmentRing"));RingMaterial=Ring.Get();
    static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> Energy(TEXT("/Game/Effects/Augments/Feedback/M_AugmentEnergy"));EnergyMaterial=Energy.Get();
    static ConstructorHelpers::FObjectFinderOptional<USoundBase> Proc(TEXT("/Game/Effects/Augments/Feedback/S_AugmentProc"));
    static ConstructorHelpers::FObjectFinderOptional<USoundBase> Cast(TEXT("/Game/Effects/Augments/Feedback/S_AugmentCast"));
    static ConstructorHelpers::FObjectFinderOptional<USoundBase> Impact(TEXT("/Game/Effects/Augments/Feedback/S_AugmentImpact"));
    static ConstructorHelpers::FObjectFinderOptional<USoundBase> Support(TEXT("/Game/Effects/Augments/Feedback/S_AugmentSupport"));
    static ConstructorHelpers::FObjectFinderOptional<USoundBase> Acquire(TEXT("/Game/Effects/Augments/Feedback/S_AugmentAcquire"));
    Sounds={Proc.Get(),Cast.Get(),Impact.Get(),Support.Get(),Acquire.Get()};
    static ConstructorHelpers::FObjectFinderOptional<USoundConcurrency> Limit(TEXT("/Game/Effects/Augments/Feedback/SC_AugmentFeedback"));Concurrency=Limit.Get();
    Attenuation=CreateDefaultSubobject<USoundAttenuation>(TEXT("AugmentAttenuation"));
    Attenuation->Attenuation.bSpatialize=true;Attenuation->Attenuation.bAttenuate=true;
    Attenuation->Attenuation.AttenuationShapeExtents=FVector(150);Attenuation->Attenuation.FalloffDistance=1650;
}
FLinearColor UDreamAugmentFeedback::Color(EAugmentID ID)
{
    switch(ID)
    {
    case EAugmentID::ContinuousAttack:case EAugmentID::AreaAttack:case EAugmentID::SkillFlame:case EAugmentID::SkillExplosion:return FLinearColor(1,.055f,.003f);
    case EAugmentID::ChillingRounds:case EAugmentID::SkillChill:case EAugmentID::SkillTimeLock:return FLinearColor(.004f,.3f,1);
    case EAugmentID::Vampire:case EAugmentID::SkillVampire:case EAugmentID::Berserker:case EAugmentID::Rampage:case EAugmentID::LastBreath:case EAugmentID::SkillExecute:return FLinearColor(.95f,.003f,.045f);
    case EAugmentID::Regeneration:case EAugmentID::SkillRegenerate:case EAugmentID::AutoCleanse:case EAugmentID::SkillCleanse:return FLinearColor(.003f,.85f,.12f);
    case EAugmentID::AutoShield:case EAugmentID::LastFortress:case EAugmentID::SkillFortress:case EAugmentID::SkillShield:case EAugmentID::SkillGuard:case EAugmentID::SkillCounter:case EAugmentID::ProjectileGuard:case EAugmentID::Resolve:return FLinearColor(1,.28f,.008f);
    case EAugmentID::ChainRounds:case EAugmentID::SkillChain:case EAugmentID::Knockback:case EAugmentID::SuppressionPulse:return FLinearColor(.015f,.08f,1);
    default:return FLinearColor(.38f,.006f,1);
    }
}
int32 UDreamAugmentFeedback::GetActiveCueCount() const
{
    int32 Count=0;for(const auto& Cue:Cues)if(Cue.IsValid()&&Cue->IsPlaying())++Count;return Count;
}
ADreamAugmentCue* UDreamAugmentFeedback::Emit(AActor* Source,EAugmentID ID,EDreamAugmentCueShape Shape,const FVector& Point,float Radius,float Seconds,AActor* Follow,const FVector& End,bool TrackState)
{
    if(!IsValid(Source)||!Source->GetWorld()||!Source->GetWorld()->IsGameWorld()||Source->GetNetMode()==NM_DedicatedServer||Seconds<=0||Point.ContainsNaN()||End.ContainsNaN())return nullptr;
    auto* World=Source->GetWorld();auto* System=World->GetSubsystem<UDreamAugmentFeedback>();
    const auto* D=Source->FindComponentByClass<UDispatchTableComponent>();
    if(!System||(D&&!D->bAugmentFeedbackEnabled))return nullptr;
    const float Time=World->GetTimeSeconds();
    System->Cues.RemoveAll([](const auto& Cue){return !Cue.IsValid()||!Cue->IsPlaying();});
    // Repeated hits refresh the same marker/aura instead of stacking geometry.
    const bool Maintained=Shape==EDreamAugmentCueShape::Aura||Shape==EDreamAugmentCueShape::Mark||Shape==EDreamAugmentCueShape::Field||Shape==EDreamAugmentCueShape::Barrier||Shape==EDreamAugmentCueShape::Decoy;
    if(Maintained)for(const auto& Cue:System->Cues)if(Cue->Matches(Source,ID,Shape,Follow)){Cue->Refresh(Seconds);return Cue.Get();}
    const auto* Def=DreamAugmentCatalog::Find(ID);const bool Priority=Def&&Def->Evolution;
    if(Time-System->Window>=.1f||Time<System->Window){System->Window=Time;System->WindowCount=0;}
    const bool Essential=Shape==EDreamAugmentCueShape::Field||Shape==EDreamAugmentCueShape::Barrier||Shape==EDreamAugmentCueShape::Decoy;
    if(Essential&&System->GetActiveCueCount()>=32)
    {
        for(const auto& Old:System->Cues)if(Old.IsValid()&&!Old->IsEssential()){UDreamObjectPoolSubsystem::ReleaseOrDestroy(Old.Get());break;}
        System->Cues.RemoveAll([](const auto& Old){return !Old.IsValid()||!Old->IsPlaying();});
    }
    if(System->GetActiveCueCount()>=(Priority||Essential?32:24)||(!Essential&&System->WindowCount>=(Priority?10:6)))return nullptr;
    if(!Priority&&!Maintained&&System->ProcReady.FindRef(ID)>Time)return nullptr;
    auto* Cue=Cast<ADreamAugmentCue>(UDreamObjectPoolSubsystem::Spawn(World,ADreamAugmentCue::StaticClass(),FTransform(Point),Source));
    if(!Cue)return nullptr;
    System->Cues.Add(Cue);++System->WindowCount;
    if(!Priority&&!Maintained)System->ProcReady.Add(ID,Time+.12f);
    Cue->Configure(Source,ID,Shape,Point,End,Radius,Seconds,Follow,TrackState,System->RingMaterial,System->EnergyMaterial,D?D->AugmentEffectOpacity:.85f);
    // Fields and markers are silent; only their original cast/impact has a sound.
    if(!Maintained&&System->SoundReady.FindRef(ID)<=Time)
    {
        System->SoundReady.Add(ID,Time+(Priority?.3f:.45f));
        const bool Support=ID==EAugmentID::Vampire||ID==EAugmentID::Regeneration||ID==EAugmentID::AutoShield||ID==EAugmentID::AutoCleanse;
        const int32 Index=Shape==EDreamAugmentCueShape::Acquire?4:Support?3:Shape==EDreamAugmentCueShape::Impact?2:Priority?1:0;
        if(System->Sounds.IsValidIndex(Index)&&System->Sounds[Index])
            UGameplayStatics::PlaySoundAtLocation(Source,System->Sounds[Index],Point,(D?D->AugmentSoundVolume:.65f)*(Priority?.75f:.45f),1,0,System->Attenuation,System->Concurrency);
    }
    return Cue;
}
void UDreamAugmentFeedback::Acquired(AActor* Source,EAugmentID ID)
{
    if(!Cast<AMainPlayerCharacter>(Source))return;const auto* Def=DreamAugmentCatalog::Find(ID);if(!Def||Def->Category!=EAugmentCategory::Active)return;
    auto* System=Source->GetWorld()->GetSubsystem<UDreamAugmentFeedback>();if(!System)return;
    FString Key;
    if(Def->Evolution&&!Def->Permanent)if(auto* D=Source->FindComponentByClass<UDispatchTableComponent>())if(auto* PC=Cast<AMainPlayerController>(CastChecked<AMainPlayerCharacter>(Source)->GetController()))Key=FString::Printf(TEXT(" · %s키 사용"),*PC->GetAugmentSkillKeyText(D->GetSkillIDs().IndexOfByKey(ID)));
    System->NoticeSource=Source;System->NoticeText=FString::Printf(TEXT("%s · %s%s"),Def->Evolution?TEXT("진화 완료"):TEXT("액티브 획득"),*Def->Name,*Key);System->NoticeUntil=Source->GetWorld()->GetTimeSeconds()+3.5f;
    Emit(Source,ID,EDreamAugmentCueShape::Acquire,Source->GetActorLocation(),100,.8f,Source);
}
FString UDreamAugmentFeedback::Notice(AActor* Source) const {return NoticeSource.Get()==Source&&GetWorld()->GetTimeSeconds()<NoticeUntil?NoticeText:FString();}
void UDreamAugmentFeedback::SkillResult(AActor* Source,EAugmentID ID,bool Success)
{
    if(Success){Failures.Remove(ID);return;}
    const auto* D=Source?Source->FindComponentByClass<UDispatchTableComponent>():nullptr;auto* R=D?D->FindRuntime():nullptr;
    if(!R||!D->HasAcquiredAugment(ID))return;
    Failures.Add(ID,GetWorld()->GetTimeSeconds()+.45f);NoticeSource=Source;
    NoticeText=FString::Printf(TEXT("%s · %s"),*D->GetAugmentDisplayName(ID).ToString(),*R->SkillState(ID));
    if(R->SkillState(ID)==TEXT("준비 완료"))NoticeText=TEXT("지금은 사용할 수 없습니다 · 이동 경로와 사용 상태를 확인하세요");
    NoticeUntil=GetWorld()->GetTimeSeconds()+1.5f;
}
void UDreamAugmentFeedback::StopSource(AActor* Source)
{
    if(!Source||!Source->GetWorld())return;auto* System=Source->GetWorld()->GetSubsystem<UDreamAugmentFeedback>();if(!System)return;
    for(const auto& Cue:System->Cues)if(Cue.IsValid()&&Cue->BelongsTo(Source))UDreamObjectPoolSubsystem::ReleaseOrDestroy(Cue.Get());
    if(System->NoticeSource.Get()==Source){System->NoticeText.Empty();System->NoticeSource.Reset();System->Failures.Empty();}
}
void UDreamAugmentFeedback::Deinitialize(){Cues.Empty();SoundReady.Empty();ProcReady.Empty();Failures.Empty();NoticeSource.Reset();Super::Deinitialize();}

ADreamAugmentCue::ADreamAugmentCue()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;PrimaryActorTick.TickInterval=1.f/30;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    RingA=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Glyph"));RingB=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EchoGlyph"));
    Sparks=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Energy"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane")),Cube(TEXT("/Engine/BasicShapes/Cube"));
    RingA->SetStaticMesh(Plane.Object);RingB->SetStaticMesh(Plane.Object);Sparks->SetStaticMesh(Cube.Object);
    for(UStaticMeshComponent* Mesh:{RingA.Get(),RingB.Get(),static_cast<UStaticMeshComponent*>(Sparks.Get())})
    {Mesh->SetupAttachment(RootComponent);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetCastShadow(false);Mesh->SetCanEverAffectNavigation(false);Mesh->bReceivesDecals=false;Mesh->SetCullDistance(5000);}
    SetActorEnableCollision(false);
}
void ADreamAugmentCue::Configure(AActor* Source,EAugmentID InID,EDreamAugmentCueShape InShape,const FVector& Point,const FVector& End,float InRadius,float Seconds,AActor* Follow,bool TrackState,UMaterialInterface* Ring,UMaterialInterface* Energy,float Opacity)
{
    Origin=Source;OriginGeneration=UDreamObjectPoolSubsystem::Generation(Source);Attachment=Follow;AttachmentGeneration=UDreamObjectPoolSubsystem::Generation(Follow);
    ID=InID;Shape=InShape;Location=Point;Destination=End;Radius=FMath::Clamp(InRadius,5.f,1500.f);Strength=FMath::Clamp(Opacity,0.f,1.f);bTrackState=TrackState;
    Started=GetWorld()->GetTimeSeconds();Until=Started+FMath::Clamp(Seconds,.05f,30.f);bPlaying=true;
    SetActorEnableCollision(false);SetActorHiddenInGame(false);SetActorTickEnabled(true);SetActorLocation(Point);SetActorRotation(Shape==EDreamAugmentCueShape::Barrier?FRotator(0,Source->GetActorRotation().Yaw,0):FRotator::ZeroRotator);Sparks->ClearInstances();
    if(!RingMID&&Ring){RingMID=UMaterialInstanceDynamic::Create(Ring,this);RingA->SetMaterial(0,RingMID);RingB->SetMaterial(0,RingMID);}
    if(!EnergyMID&&Energy){EnergyMID=UMaterialInstanceDynamic::Create(Energy,this);Sparks->SetMaterial(0,EnergyMID);}
    const FLinearColor Tint=UDreamAugmentFeedback::Color(ID);if(RingMID)RingMID->SetVectorParameterValue(TEXT("Tint"),Tint*1.25f);if(EnergyMID)EnergyMID->SetVectorParameterValue(TEXT("Tint"),Tint*1.25f);
    RingA->SetVisibility(Shape!=EDreamAugmentCueShape::Beam);RingB->SetVisibility(Shape==EDreamAugmentCueShape::Impact||Shape==EDreamAugmentCueShape::Mark||Shape==EDreamAugmentCueShape::Acquire||Shape==EDreamAugmentCueShape::Barrier||(Shape==EDreamAugmentCueShape::Aura&&(ID==EAugmentID::SkillShield||ID==EAugmentID::SkillGuard||ID==EAugmentID::SkillCounter||ID==EAugmentID::SkillFortress||ID==EAugmentID::AutoShield)));
    if(Shape==EDreamAugmentCueShape::Beam)
    {
        const FVector Delta=End-Point;const FVector Side=FVector::CrossProduct(Delta.GetSafeNormal(),FVector::UpVector).GetSafeNormal()*12;
        for(int32 I=0;I<3;++I){const FVector A=FMath::Lerp(FVector::ZeroVector,Delta,I/3.f)+(I==1?Side:I==2?-Side:FVector::ZeroVector);const FVector B=FMath::Lerp(FVector::ZeroVector,Delta,(I+1)/3.f)+(I==0?Side:I==1?-Side:FVector::ZeroVector);const FVector V=B-A;Sparks->AddInstance(FTransform(V.Rotation(),(A+B)*.5f,FVector(V.Size()/100,.04f,.04f)));}
    }
    else if(Shape==EDreamAugmentCueShape::Barrier)
    {
        // The visible frame matches the real 3 m x 2 m projectile barrier.
        for(float Z:{-100.f,100.f})Sparks->AddInstance(FTransform(FRotator(0,90,0),FVector(0,0,Z),FVector(3,.018f,.018f)));
        for(float Y:{-150.f,150.f})Sparks->AddInstance(FTransform(FRotator(90,0,0),FVector(0,Y,0),FVector(2,.018f,.018f)));
    }
    else if(Shape==EDreamAugmentCueShape::Impact||Shape==EDreamAugmentCueShape::Acquire)for(int32 I=0;I<6;++I)Sparks->AddInstance(FTransform::Identity);
    UpdateVisual();
}
void ADreamAugmentCue::Refresh(float Seconds){Until=FMath::Max(Until,GetWorld()->GetTimeSeconds()+FMath::Clamp(Seconds,.05f,30.f));}
bool ADreamAugmentCue::Matches(AActor* Source,EAugmentID InID,EDreamAugmentCueShape InShape,AActor* Follow)const
{return bPlaying&&Origin.Get()==Source&&ID==InID&&Shape==InShape&&Attachment.Get()==Follow&&AttachmentGeneration==UDreamObjectPoolSubsystem::Generation(Follow);}
void ADreamAugmentCue::Tick(float)
{
    auto* Source=Origin.Get();auto* Follow=Attachment.Get();auto* Stats=Source?Source->FindComponentByClass<UCombatStatsComponent>():nullptr;
    bool Valid=Source&&GetWorld()->GetTimeSeconds()<Until&&OriginGeneration==UDreamObjectPoolSubsystem::Generation(Source)&&(!Stats||!Stats->IsDead());
    if(Attachment.IsExplicitlyNull()==false)Valid=Valid&&Follow&&AttachmentGeneration==UDreamObjectPoolSubsystem::Generation(Follow)&&!Follow->IsHidden();
    if(Follow&&Follow!=Source)if(auto* S=Follow->FindComponentByClass<UCombatStatsComponent>())Valid=Valid&&!S->IsDead();
    if(Valid&&bTrackState){auto* D=Source->FindComponentByClass<UDispatchTableComponent>();Valid=D&&D->FindRuntime()&&D->FindRuntime()->HasFeedbackState(ID);}
    if(!Valid){UDreamObjectPoolSubsystem::ReleaseOrDestroy(this);return;}UpdateVisual();
}
void ADreamAugmentCue::UpdateVisual()
{
    const float Time=GetWorld()->GetTimeSeconds(),Life=FMath::Max(.05f,Until-Started),T=FMath::Clamp((Time-Started)/Life,0.f,1.f);
    const bool Persistent=Shape==EDreamAugmentCueShape::Aura||Shape==EDreamAugmentCueShape::Field||Shape==EDreamAugmentCueShape::Mark||Shape==EDreamAugmentCueShape::Barrier||Shape==EDreamAugmentCueShape::Decoy;
    const float Fade=Persistent?FMath::Min(1.f,(Until-Time)/.2f)*(.72f+.12f*FMath::Sin(Time*4)):FMath::Square(1-T);
    if(RingMID)RingMID->SetScalarParameterValue(TEXT("Alpha"),Strength*Fade);if(EnergyMID)EnergyMID->SetScalarParameterValue(TEXT("Alpha"),Strength*Fade);
    FVector Point=Location;
    if(auto* Follow=Attachment.Get())
    {
        Point=Follow->GetActorLocation();
        if(Shape==EDreamAugmentCueShape::Aura||Shape==EDreamAugmentCueShape::Acquire)
        {if(auto* P=Cast<ACharacter>(Follow))Point.Z-=P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-5;}
        if(Shape==EDreamAugmentCueShape::Mark)Point.Z+=55;
    }
    SetActorLocation(Point);const float Size=Radius*2/(100*.88f)*(Persistent?1:FMath::Lerp(.3f,1.f,Shape==EDreamAugmentCueShape::Ring?FMath::Min(1.f,T*3):T));
    RingA->SetRelativeLocation(FVector::ZeroVector);RingA->SetRelativeRotation(FRotator::ZeroRotator);RingA->SetRelativeScale3D(FVector(Size));
    RingB->SetRelativeLocation(FVector(0,0,3));RingB->SetRelativeRotation(FRotator(0,Time*24,0));RingB->SetRelativeScale3D(FVector(Size*.65f));
    if(Shape==EDreamAugmentCueShape::Aura||Shape==EDreamAugmentCueShape::Field)RingA->SetRelativeRotation(FRotator(0,Time*18,0));
    if(Shape==EDreamAugmentCueShape::Aura&&RingB->IsVisible())
    {RingB->SetRelativeLocation(FVector(0,0,75));RingB->SetRelativeRotation(FRotator(90,Origin.IsValid()?Origin->GetActorRotation().Yaw:0,0));RingB->SetRelativeScale3D(FVector(1.6f,1.2f,1));}
    if(Shape==EDreamAugmentCueShape::Impact||Shape==EDreamAugmentCueShape::Mark)
    {RingA->SetRelativeRotation(FRotator(90,0,0));RingB->SetRelativeRotation(FRotator(0,0,90));}
    if(Shape==EDreamAugmentCueShape::Barrier||Shape==EDreamAugmentCueShape::Decoy)
    {RingA->SetRelativeRotation(FRotator(90,0,0));RingB->SetRelativeRotation(FRotator(0,0,90));
        if(Shape==EDreamAugmentCueShape::Barrier){RingA->SetRelativeScale3D(FVector(2,3,1));RingB->SetRelativeScale3D(FVector(.7f));}}
    if(Shape==EDreamAugmentCueShape::Impact||Shape==EDreamAugmentCueShape::Acquire)
    for(int32 I=0;I<6;++I)
    {const float A=I*UE_TWO_PI/6;const FVector Dir(FMath::Cos(A),FMath::Sin(A),Shape==EDreamAugmentCueShape::Acquire?.4f:.2f);Sparks->UpdateInstanceTransform(I,FTransform(Dir.Rotation(),Dir*Radius*(.3f+T),FVector(.12f,.018f,.018f)*(1-T)),false,false);}
    if(Sparks->GetInstanceCount()>0&&(Shape==EDreamAugmentCueShape::Impact||Shape==EDreamAugmentCueShape::Acquire))Sparks->MarkRenderStateDirty();
}
void ADreamAugmentCue::PrepareForPoolReuse(){OnPoolReturned();}
void ADreamAugmentCue::OnPoolAcquired(){SetActorEnableCollision(false);}
void ADreamAugmentCue::OnPoolReturned()
{bPlaying=false;bTrackState=false;Origin.Reset();Attachment.Reset();Sparks->ClearInstances();RingA->SetVisibility(false);RingB->SetVisibility(false);SetActorTickEnabled(false);SetActorEnableCollision(false);}
