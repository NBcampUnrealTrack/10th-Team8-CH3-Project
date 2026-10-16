#include "DreamAugmentFeedback.h"
#include "DreamPlayPolish.h"
#include "MonsterBase.h"
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
#include "Components/PoseableMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundConcurrency.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
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
    case EAugmentID::ChillingRounds:case EAugmentID::SkillChill:case EAugmentID::SkillTimeLock:return FLinearColor(.16f,.65f,1);
    case EAugmentID::Vampire:case EAugmentID::SkillVampire:case EAugmentID::Berserker:case EAugmentID::Rampage:case EAugmentID::LastBreath:case EAugmentID::SkillExecute:return FLinearColor(.95f,.003f,.045f);
    case EAugmentID::Regeneration:case EAugmentID::SkillRegenerate:case EAugmentID::AutoCleanse:case EAugmentID::SkillCleanse:return FLinearColor(.003f,.85f,.12f);
    case EAugmentID::AutoShield:case EAugmentID::LastFortress:case EAugmentID::SkillFortress:case EAugmentID::SkillShield:case EAugmentID::SkillGuard:case EAugmentID::SkillCounter:case EAugmentID::ProjectileGuard:case EAugmentID::Resolve:return FLinearColor(1,.28f,.008f);
    case EAugmentID::ChainRounds:case EAugmentID::SkillChain:case EAugmentID::Knockback:case EAugmentID::SuppressionPulse:return FLinearColor(.015f,.08f,1);
    case EAugmentID::PiercingRounds:case EAugmentID::SkillPiercing:return FLinearColor(.04f,.85f,1.f);
    case EAugmentID::FocusFire:case EAugmentID::SkillOvercharge:return FLinearColor(1.f,.65f,.12f);
    case EAugmentID::FragmentRounds:case EAugmentID::SkillScatter:return FLinearColor(1.f,.3f,.6f);
    case EAugmentID::RiftRounds:case EAugmentID::GravityMark:case EAugmentID::SealRounds:case EAugmentID::SkillRift:case EAugmentID::SkillSeal:case EAugmentID::SkillGravity:return FLinearColor(.65f,.15f,1.f);
    case EAugmentID::CrisisEscape:case EAugmentID::SprintBoost:case EAugmentID::RetreatTactics:case EAugmentID::MobileSupport:case EAugmentID::SkillSprint:case EAugmentID::SkillMobileFire:return FLinearColor(.03f,1.f,.6f);
    default:return FLinearColor(.65f,.12f,1);
    }
}
int32 UDreamAugmentFeedback::GetActiveCueCount() const
{
    int32 Count=0;for(const auto& Cue:Cues)if(Cue.IsValid()&&Cue->IsPlaying())++Count;return Count;
}
ADreamAugmentCue* UDreamAugmentFeedback::Emit(AActor* Source,EAugmentID ID,EDreamAugmentCueShape Shape,const FVector& Point,float Radius,float Seconds,AActor* Follow,const FVector& End,bool TrackState)
{
    if(!IsValid(Source)||!Source->GetWorld()||!Source->GetWorld()->IsGameWorld()||Source->GetNetMode()==NM_DedicatedServer||Seconds<=0||Point.ContainsNaN()||End.ContainsNaN())return nullptr;
    if(Cast<AMainPlayerCharacter>(Source)&&Shape!=EDreamAugmentCueShape::Acquire&&Shape!=EDreamAugmentCueShape::Field&&Shape!=EDreamAugmentCueShape::Barrier&&Shape!=EDreamAugmentCueShape::Decoy)
    {if(Shape==EDreamAugmentCueShape::Impact||Shape==EDreamAugmentCueShape::Ring||Shape==EDreamAugmentCueShape::Beam)Seconds=FMath::Max(Seconds,.55f);}
    auto* World=Source->GetWorld();auto* System=World->GetSubsystem<UDreamAugmentFeedback>();
    const auto* D=Source->FindComponentByClass<UDispatchTableComponent>();
    if(!System||(D&&!D->bAugmentFeedbackEnabled))return nullptr;
    const auto* Pref=World->GetGameInstance()?World->GetGameInstance()->GetSubsystem<UDreamPlayPreferences>():nullptr;
    const bool Simple=Pref&&Pref->IsSimplifiedEffects();
    const float Time=World->GetTimeSeconds();
    System->Cues.RemoveAll([](const auto& Cue){return !Cue.IsValid()||!Cue->IsPlaying();});
    // Repeated hits refresh the same marker/aura instead of stacking geometry.
    const bool Maintained=Shape==EDreamAugmentCueShape::Aura||Shape==EDreamAugmentCueShape::Mark||Shape==EDreamAugmentCueShape::Field||Shape==EDreamAugmentCueShape::Barrier||Shape==EDreamAugmentCueShape::Decoy;
    if(Maintained)for(const auto& Cue:System->Cues)if(Cue->Matches(Source,ID,Shape,Follow)){Cue->Refresh(Seconds);return Cue.Get();}
    const auto* Def=DreamAugmentCatalog::Find(ID);const bool Priority=Def&&Def->Evolution;
    const uint16 ProcKey=(static_cast<uint16>(ID)<<4)|static_cast<uint16>(Shape);
    const bool FragmentLinks=ID==EAugmentID::FragmentRounds&&Shape==EDreamAugmentCueShape::Beam;
    if(Time-System->Window>=.1f||Time<System->Window){System->Window=Time;System->WindowCount=0;}
    const bool Essential=Shape==EDreamAugmentCueShape::Field||Shape==EDreamAugmentCueShape::Barrier||Shape==EDreamAugmentCueShape::Decoy||Cast<AMonsterBase>(Source)!=nullptr;
    if(Essential&&System->GetActiveCueCount()>=32)
    {
        for(const auto& Old:System->Cues)if(Old.IsValid()&&!Old->IsEssential()){UDreamObjectPoolSubsystem::ReleaseOrDestroy(Old.Get());break;}
        System->Cues.RemoveAll([](const auto& Old){return !Old.IsValid()||!Old->IsPlaying();});
    }
    if(System->GetActiveCueCount()>=(Priority||Essential?32:(Simple?12:24))||(!Essential&&System->WindowCount>=(Priority?10:Simple?3:6)))return nullptr;
    if(Simple&&!Priority&&!Essential&&!FragmentLinks&&System->ProcReady.FindRef(ProcKey)>Time)return nullptr;
    if(!Priority&&!Maintained&&!FragmentLinks&&System->ProcReady.FindRef(ProcKey)>Time)return nullptr;
    auto* Cue=Cast<ADreamAugmentCue>(UDreamObjectPoolSubsystem::Spawn(World,ADreamAugmentCue::StaticClass(),FTransform(Point),Source));
    if(!Cue)return nullptr;
    System->Cues.Add(Cue);++System->WindowCount;
    if(!Priority&&!Maintained)System->ProcReady.Add(ProcKey,Time+(Simple?.3f:.12f));
    const bool ThreatNearby=System->Cues.ContainsByPredicate([](const auto& C){return C.IsValid()&&C->bThreat&&C->IsPlaying();});
    Cue->Configure(Source,ID,Shape,Point,End,Radius,Seconds,Follow,TrackState,System->RingMaterial,System->EnergyMaterial,(D?D->AugmentEffectOpacity:.85f)*(Simple&&!Essential?.65f:1.f)*(ThreatNearby&&!Essential?.55f:1.f));
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
    Motifs=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SignatureMotes"));Motifs->SetupAttachment(RootComponent);Motifs->SetStaticMesh(Plane.Object);
    Motifs->SetCollisionEnabled(ECollisionEnabled::NoCollision);Motifs->SetCastShadow(false);Motifs->SetCanEverAffectNavigation(false);Motifs->bReceivesDecals=false;Motifs->SetCullDistance(5000);
    Ghost=CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("DecoySilhouette"));Ghost->SetupAttachment(RootComponent);Ghost->SetCollisionEnabled(ECollisionEnabled::NoCollision);Ghost->SetCastShadow(false);Ghost->SetCanEverAffectNavigation(false);Ghost->SetCullDistance(5000);Ghost->bReceivesDecals=false;Ghost->SetComponentTickEnabled(false);Ghost->SetVisibility(false);
    static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface> Glyph(TEXT("/Game/Effects/Augments/Feedback/M_AugmentSignature")),Echo(TEXT("/Game/Effects/Augments/Feedback/M_AugmentGhost"));SignatureMaterial=Glyph.Get();GhostMaterial=Echo.Get();
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
    RingA->SetMaterial(0,RingMID);RingB->SetMaterial(0,RingMID);
    bThreat=Cast<AMonsterBase>(Source)!=nullptr;
    const FLinearColor Tint=UDreamAugmentFeedback::Color(ID);if(RingMID)RingMID->SetVectorParameterValue(TEXT("Tint"),Tint*1.25f);if(EnergyMID)EnergyMID->SetVectorParameterValue(TEXT("Tint"),Tint*1.25f);
    RingA->SetVisibility(Shape!=EDreamAugmentCueShape::Beam);RingB->SetVisibility(Shape==EDreamAugmentCueShape::Impact||Shape==EDreamAugmentCueShape::Mark||Shape==EDreamAugmentCueShape::Acquire||Shape==EDreamAugmentCueShape::Barrier||(Shape==EDreamAugmentCueShape::Aura&&(ID==EAugmentID::SkillShield||ID==EAugmentID::SkillGuard||ID==EAugmentID::SkillCounter||ID==EAugmentID::SkillFortress||ID==EAugmentID::AutoShield||ID==EAugmentID::ProjectileGuard||ID==EAugmentID::Resolve||ID==EAugmentID::LastFortress)));
    if(Shape==EDreamAugmentCueShape::Beam)
    {
        const FVector Delta=End-Point;
        const bool Chain=ID==EAugmentID::ChainRounds||ID==EAugmentID::SkillChain||ID==EAugmentID::GravityMark;
        const int32 Count=Chain?7:1;const FVector Side=FVector::CrossProduct(Delta.GetSafeNormal(),FVector::UpVector).GetSafeNormal()*35;
        FVector A=FVector::ZeroVector;
        for(int32 I=0;I<Count;++I)
        {
            const FVector B=FMath::Lerp(FVector::ZeroVector,Delta,(I+1.f)/Count)+(I==Count-1?FVector::ZeroVector:Side*(I%2?1.f:-1.f));
            const FVector V=B-A;Sparks->AddInstance(FTransform(V.Rotation(),(A+B)*.5f,FVector(V.Size()/100,.10f,.10f)));A=B;
        }
    }
    else if(Shape==EDreamAugmentCueShape::Barrier)
    {
        // The visible frame matches the real 3 m x 2 m projectile barrier.
        for(float Z:{-100.f,100.f})Sparks->AddInstance(FTransform(FRotator(0,90,0),FVector(0,0,Z),FVector(3,.018f,.018f)));
        for(float Y:{-150.f,150.f})Sparks->AddInstance(FTransform(FRotator(90,0,0),FVector(0,Y,0),FVector(2,.018f,.018f)));
    }
    else if(Shape==EDreamAugmentCueShape::Impact||Shape==EDreamAugmentCueShape::Acquire||(!bThreat&&(Shape==EDreamAugmentCueShape::Aura||Shape==EDreamAugmentCueShape::Mark)))for(int32 I=0;I<6;++I)Sparks->AddInstance(FTransform::Identity);
    ConfigureSignatureVisuals();UpdateVisual();
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
    const float Fade=Persistent?FMath::Min(1.f,(Until-Time)/.2f)*(.72f+.12f*FMath::Sin(Time*4)):FMath::Pow(1-T,.7f);
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
    if(UpdateSignatureVisuals(Time,T,Fade,Point))return;
    if(Shape==EDreamAugmentCueShape::Aura&&RingB->IsVisible())
    {RingB->SetRelativeLocation(FVector(0,0,75));RingB->SetRelativeRotation(FRotator(90,Origin.IsValid()?Origin->GetActorRotation().Yaw:0,0));RingB->SetRelativeScale3D(FVector(1.6f,1.2f,1));}
    if(Shape==EDreamAugmentCueShape::Impact||Shape==EDreamAugmentCueShape::Mark)
    {
        if(bThreat){RingA->SetRelativeRotation(FRotator(90,0,0));RingB->SetRelativeRotation(FRotator(0,0,90));}
        else if(auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0))
        {
            const FRotator Facing=FRotationMatrix::MakeFromZ(Camera->GetCameraLocation()-Point).Rotator();
            RingA->SetRelativeRotation(Facing);RingB->SetRelativeRotation(Facing+FRotator(0,0,45));
            RingB->SetRelativeLocation((Camera->GetCameraLocation()-Point).GetSafeNormal()*3);
        }
    }
    if(Shape==EDreamAugmentCueShape::Barrier||Shape==EDreamAugmentCueShape::Decoy)
    {RingA->SetRelativeRotation(FRotator(90,0,0));RingB->SetRelativeRotation(FRotator(0,0,90));
        if(Shape==EDreamAugmentCueShape::Barrier){RingA->SetRelativeScale3D(FVector(2,3,1));RingB->SetRelativeScale3D(FVector(.7f));}}
    if(Shape==EDreamAugmentCueShape::Impact||Shape==EDreamAugmentCueShape::Acquire)
    for(int32 I=0;I<6;++I)
    {const float A=I*UE_TWO_PI/6;const FVector Dir(FMath::Cos(A),FMath::Sin(A),Shape==EDreamAugmentCueShape::Acquire?.4f:.2f);Sparks->UpdateInstanceTransform(I,FTransform(Dir.Rotation(),Dir*Radius*(.3f+T),FVector(.30f,.055f,.055f)*(1-T)),false,false);}
    if(!bThreat&&(Shape==EDreamAugmentCueShape::Aura||Shape==EDreamAugmentCueShape::Mark))
    {
        const bool Moving=ID==EAugmentID::CrisisEscape||ID==EAugmentID::SprintBoost||ID==EAugmentID::RetreatTactics||ID==EAugmentID::MobileSupport||ID==EAugmentID::SkillSprint||ID==EAugmentID::SkillMobileFire;
        const bool Healing=ID==EAugmentID::Vampire||ID==EAugmentID::Regeneration||ID==EAugmentID::AutoCleanse||ID==EAugmentID::SkillRegenerate;
        const float Orbit=Shape==EDreamAugmentCueShape::Mark?Radius*.8f:Radius;
        for(int32 I=0;I<6;++I)
        {
            const float Angle=I*UE_TWO_PI/6+Time*(Moving?4.f:1.1f);
            FVector At(FMath::Cos(Angle)*Orbit,FMath::Sin(Angle)*Orbit,Shape==EDreamAugmentCueShape::Aura?35+FMath::Fmod(Time*45+I*20,100.f):FMath::Sin(Angle*2)*30);
            FVector Scale(.10f,.10f,.30f);FRotator Rotation(0,Time*40,0);
            if(Healing){Scale=I%2?FVector(.08f,.08f,.28f):FVector(.28f,.08f,.08f);At.Z=30+FMath::Fmod(Time*70+I*17,95.f);}
            if(Moving){Scale=FVector(.38f,.06f,.06f);Rotation=FRotator(0,Origin.IsValid()?Origin->GetActorRotation().Yaw:0,0);}
            Sparks->UpdateInstanceTransform(I,FTransform(Rotation,At,Scale),false,false);
        }
        if(Shape==EDreamAugmentCueShape::Mark)
        {
            auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0);
            const FQuat Facing=FRotationMatrix::MakeFromZ(Camera?Camera->GetCameraLocation()-Point:FVector::UpVector).ToQuat();
            for(int32 I=0;I<6;++I)
            {
                FVector A,B;
                if(ID==EAugmentID::ChillingRounds||ID==EAugmentID::SkillChill)
                {const float Angle=I*UE_TWO_PI/6;const FVector D(FMath::Cos(Angle),FMath::Sin(Angle),0);A=D*Radius*.24f;B=D*Radius*.8f;}
                else if(ID==EAugmentID::SealRounds||ID==EAugmentID::SkillSeal||ID==EAugmentID::StopMark)
                {if(I<2){A=FVector((I?1.f:-1.f)*Radius*.45f,-Radius*.7f,0);B=A+FVector(0,Radius*1.4f,0);}else{A=FVector(-Radius*.65f,Radius*(-.6f+(I-2)*.4f),0);B=A+FVector(Radius*1.3f,0,0);}}
                else
                {const float Angle=I*UE_TWO_PI/6;A=FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*Radius*.8f;B=FVector(FMath::Cos(Angle+UE_TWO_PI/6),FMath::Sin(Angle+UE_TWO_PI/6),0)*Radius*.8f;}
                A=Facing.RotateVector(A);B=Facing.RotateVector(B);const FVector Delta=B-A;
                Sparks->UpdateInstanceTransform(I,FTransform(Delta.Rotation(),(A+B)*.5f,FVector(Delta.Size()/100,.05f,.05f)),false,false);
            }
        }
    }
    if(Sparks->GetInstanceCount()>0&&Shape!=EDreamAugmentCueShape::Beam&&Shape!=EDreamAugmentCueShape::Barrier)Sparks->MarkRenderStateDirty();
}
void ADreamAugmentCue::PrepareForPoolReuse(){OnPoolReturned();}
void ADreamAugmentCue::OnPoolAcquired(){SetActorEnableCollision(false);}
void ADreamAugmentCue::OnPoolReturned()
{bPlaying=false;bTrackState=false;bThreat=false;Origin.Reset();Attachment.Reset();Sparks->ClearInstances();Motifs->ClearInstances();Ghost->SetVisibility(false);Ghost->SetSkinnedAssetAndUpdate(nullptr);Ghost->EmptyOverrideMaterials();SignatureStyle=0;RingA->SetVisibility(false);RingB->SetVisibility(false);SetActorTickEnabled(false);SetActorEnableCollision(false);}

void ADreamAugmentCue::ThreatTint(const FLinearColor& Color)
{bThreat=true;if(RingMID)RingMID->SetVectorParameterValue(TEXT("Tint"),Color);if(EnergyMID)EnergyMID->SetVectorParameterValue(TEXT("Tint"),Color);}
