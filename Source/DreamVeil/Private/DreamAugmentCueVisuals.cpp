#include "DreamAugmentFeedback.h"
#include "DreamAugmentCatalog.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
namespace
{
int32 GlyphStyle(EAugmentID ID)
{
    switch(ID)
    {
    case EAugmentID::ChillingRounds:case EAugmentID::SkillChill:case EAugmentID::SkillTimeLock:return 1;
    case EAugmentID::GravityMark:case EAugmentID::SkillGravity:return 2;
    case EAugmentID::SealRounds:case EAugmentID::SkillSeal:case EAugmentID::StopMark:return 3;
    case EAugmentID::RiftRounds:case EAugmentID::SkillRift:return 4;
    case EAugmentID::KillTrap:case EAugmentID::SkillTrap:case EAugmentID::SuppressionEcho:case EAugmentID::SkillSuppress:return 5;
    case EAugmentID::Vampire:case EAugmentID::SkillVampire:case EAugmentID::Regeneration:case EAugmentID::SkillRegenerate:case EAugmentID::AutoCleanse:case EAugmentID::SkillCleanse:return 6;
    case EAugmentID::AutoShield:case EAugmentID::LastFortress:case EAugmentID::SkillFortress:case EAugmentID::SkillShield:case EAugmentID::SkillGuard:case EAugmentID::SkillCounter:case EAugmentID::ProjectileGuard:case EAugmentID::Resolve:case EAugmentID::Riposte:case EAugmentID::ThornArmor:return 7;
    case EAugmentID::CrisisEscape:case EAugmentID::SprintBoost:case EAugmentID::RetreatTactics:case EAugmentID::MobileSupport:case EAugmentID::SkillSprint:case EAugmentID::SkillMobileFire:return 8;
    case EAugmentID::FocusFire:case EAugmentID::ExecutionRounds:case EAugmentID::SkillExecute:case EAugmentID::SkillOvercharge:return 9;
    default:return 0;
    }
}
}
void ADreamAugmentCue::ConfigureSignatureVisuals()
{
    Motifs->ClearInstances();Ghost->SetVisibility(false);Ghost->SetSkinnedAssetAndUpdate(nullptr);Ghost->EmptyOverrideMaterials();
    SignatureStyle=bThreat?0:GlyphStyle(ID);
    if(bThreat)return; // Enemy danger telegraphs retain their original material, shape and timing.
    if(!SignatureMID&&SignatureMaterial)SignatureMID=UMaterialInstanceDynamic::Create(SignatureMaterial,this);
    if(!GhostMID&&GhostMaterial)GhostMID=UMaterialInstanceDynamic::Create(GhostMaterial,this);
    const auto Color=UDreamAugmentFeedback::Color(ID);if(SignatureMID){SignatureMID->SetVectorParameterValue(TEXT("Tint"),Color);SignatureMID->SetScalarParameterValue(TEXT("Style"),SignatureStyle);SignatureMID->SetScalarParameterValue(TEXT("Glow"),Shape==EDreamAugmentCueShape::Aura?3.f:5.f);}
    if(GhostMID){GhostMID->SetVectorParameterValue(TEXT("Tint"),ID==EAugmentID::Vampire?FLinearColor(1,.025f,.10f):FLinearColor(.18f,.45f,1));GhostMID->SetScalarParameterValue(TEXT("Glow"),4.5f);}
    if(Shape==EDreamAugmentCueShape::Decoy)
    {
        RingB->SetVisibility(false);Sparks->ClearInstances();
        if(SignatureMID){SignatureMID->SetScalarParameterValue(TEXT("Style"),2);RingA->SetMaterial(0,SignatureMID);}
        auto* Character=Cast<ACharacter>(Origin.Get());int32 VisibleGhosts=0;
        for(TActorIterator<ADreamAugmentCue> It(GetWorld());It;++It)if(*It!=this&&It->IsPlaying()&&It->Ghost&&It->Ghost->IsVisible())++VisibleGhosts;
        // Frozen pose, one low-detail mesh per decoy, with a hard two-silhouette ceiling.
        if(Character&&Character->GetMesh()->GetSkeletalMeshAsset()&&GhostMID&&VisibleGhosts<2)
        {
            auto* SourceMesh=Character->GetMesh();Ghost->SetSkinnedAssetAndUpdate(SourceMesh->GetSkeletalMeshAsset());Ghost->SetForcedLOD(3);Ghost->CopyPoseFromSkeletalComponent(SourceMesh);Ghost->RefreshBoneTransforms();Ghost->UpdateBounds();
            Ghost->SetRelativeLocation(FVector(0,0,-50));Ghost->SetWorldRotation(SourceMesh->GetComponentRotation());Ghost->SetWorldScale3D(SourceMesh->GetComponentScale());
            for(int32 I=0;I<Ghost->GetNumMaterials();++I)Ghost->SetMaterial(I,GhostMID);
            Ghost->SetVisibility(true);Ghost->SetComponentTickEnabled(false);
        }
        return;
    }
    const bool Stream=Shape==EDreamAugmentCueShape::Siphon;
    const bool GravityLink=SignatureStyle==2&&Shape==EDreamAugmentCueShape::Beam;
    if(Stream||GravityLink)
    {
        RingA->SetVisibility(false);RingB->SetVisibility(false);Sparks->ClearInstances();
        Motifs->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));Motifs->SetMaterial(0,Stream?EnergyMID:GhostMID);
        if(GhostMID)GhostMID->SetVectorParameterValue(TEXT("Tint"),Stream?FLinearColor(1,.025f,.07f):Color);
        for(int32 I=0;I<12;++I)Motifs->AddInstance(FTransform::Identity);return;
    }
    if(!SignatureStyle||!SignatureMID||Shape==EDreamAugmentCueShape::Acquire||Shape==EDreamAugmentCueShape::Beam||Shape==EDreamAugmentCueShape::Barrier)return;
    Sparks->ClearInstances();RingA->SetMaterial(0,SignatureMID);RingB->SetVisibility(false);
    Motifs->SetStaticMesh(RingA->GetStaticMesh());Motifs->SetMaterial(0,SignatureMID);
    const int32 Count=SignatureStyle==1?12:SignatureStyle==2?16:SignatureStyle==6?6:SignatureStyle==8?6:0;
    for(int32 I=0;I<Count;++I)Motifs->AddInstance(FTransform::Identity);
}
bool ADreamAugmentCue::UpdateSignatureVisuals(float Time,float T,float Fade,const FVector& Point)
{
    if(bThreat)return false;
    const float Age=Time-Started;const auto* Definition=DreamAugmentCatalog::Find(ID);const bool Evolved=Definition&&Definition->Evolution;
    if(SignatureMID)SignatureMID->SetScalarParameterValue(TEXT("Alpha"),Strength*Fade*(Evolved?1.f:.9f));
    if(GhostMID)GhostMID->SetScalarParameterValue(TEXT("Alpha"),Strength*Fade*.8f);
    auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0);
    const FQuat Facing=FRotationMatrix::MakeFromZ(Camera?Camera->GetCameraLocation()-Point:FVector::UpVector).ToQuat();
    if(Shape==EDreamAugmentCueShape::Decoy)
    {
        RingA->SetRelativeLocation(FVector(0,0,-40));RingA->SetRelativeRotation(FRotator::ZeroRotator);RingA->SetRelativeScale3D(FVector(1.1f));
        return true;
    }
    if(Shape==EDreamAugmentCueShape::Siphon||(SignatureStyle==2&&Shape==EDreamAugmentCueShape::Beam))
    {
        const bool Siphon=Shape==EDreamAugmentCueShape::Siphon;const FVector End=Siphon&&Origin.IsValid()?Origin->GetActorLocation()+FVector(0,0,15):Destination;
        const FVector Delta=End-Location;const FVector Side=FVector::CrossProduct(Delta.GetSafeNormal(),FVector::UpVector).GetSafeNormal();
        for(int32 I=0;I<Motifs->GetInstanceCount();++I)
        {
            const float U=FMath::Clamp((T-I*.018f)/.76f,0.f,1.f);const float Arc=FMath::Sin(U*UE_PI);const float A=I*2.399f+Age*5;
            const FVector At=Delta*U+Side*(FMath::Sin(A)*Arc*(Siphon?28.f:40.f))+FVector(0,0,Arc*(Siphon?40.f:18.f));
            const float Size=(U<=0||U>=1)?0.f:(Siphon?.09f:.065f)*(1-.45f*U);
            Motifs->UpdateInstanceTransform(I,FTransform(FQuat::Identity,At,FVector(Size)),false,false);
        }
        Motifs->MarkRenderStateDirty();return true;
    }
    if(!SignatureStyle||!SignatureMID||Shape==EDreamAugmentCueShape::Acquire||Shape==EDreamAugmentCueShape::Beam||Shape==EDreamAugmentCueShape::Barrier)return false;
    const bool Ground=Shape==EDreamAugmentCueShape::Field||Shape==EDreamAugmentCueShape::Ring;
    const bool Aura=Shape==EDreamAugmentCueShape::Aura;
    // Preserve exact field radius with the existing outer boundary, unique glyph inside it.
    if(Shape==EDreamAugmentCueShape::Field)
    {
        RingB->SetVisibility(true);RingB->SetMaterial(0,RingMID);RingB->SetRelativeRotation(FRotator::ZeroRotator);RingB->SetRelativeScale3D(FVector(Radius*2/(100*.88f)));RingB->SetRelativeLocation(FVector(0,0,3));
    }
    else RingB->SetVisibility(false);
    if(Ground){RingA->SetRelativeRotation(FRotator(0,SignatureStyle==2?Age*35:0,0));RingA->SetRelativeLocation(FVector(0,0,5));}
    else{RingA->SetRelativeRotation(Facing);const FVector Front=Camera?(Camera->GetCameraLocation()-Point).GetSafeNormal()*65:FVector::ZeroVector;RingA->SetRelativeLocation(Front+FVector(0,0,Aura?65:-5));}
    const float GlyphRadius=Ground?Radius:Aura?FMath::Min(Radius,30.f):FMath::Min(Radius,75.f);
    RingA->SetRelativeScale3D(FVector(GlyphRadius*2/100));
    const int32 Count=Motifs->GetInstanceCount();
    for(int32 I=0;I<Count;++I)
    {
        const float Seed=I/FMath::Max(1.f,float(Count));const float A=I*2.399f+Age*(SignatureStyle==2?-2.f:.25f);FVector At;float Size=.12f;
        if(SignatureStyle==1)
        {
            const float Orbit=Radius*(.65f+.18f*FMath::Sin(I*3.f));At=FVector(FMath::Cos(A)*Orbit,FMath::Sin(A)*Orbit,-65+FMath::Fmod(Age*24+Seed*145,145.f));Size=.13f+(I%3)*.035f;
        }
        else if(SignatureStyle==2)
        {
            const float U=1-FMath::Frac(Age*.7f+Seed);const float Orbit=Radius*(.15f+.7f*U);At=FVector(FMath::Cos(A)*Orbit,FMath::Sin(A)*Orbit,Ground?10: -40+U*80);Size=.07f+.10f*U;
        }
        else if(SignatureStyle==6)
        {
            At=FVector(FMath::Cos(A)*Radius*.65f,FMath::Sin(A)*Radius*.65f,20+FMath::Fmod(Age*65+Seed*100,100.f));Size=.22f;
        }
        else
        {
            const float Yaw=Origin.IsValid()?Origin->GetActorRotation().Yaw:0;const FVector Forward=FRotator(0,Yaw,0).Vector();At= -Forward*(30+Seed*65)+FVector(0,(I%2?1:-1)*35,12+FMath::Fmod(Age*40+I*15,45.f));Size=.30f;
        }
        Motifs->UpdateInstanceTransform(I,FTransform(Facing,At,FVector(Size)),false,false);
    }
    if(Count)Motifs->MarkRenderStateDirty();return true;
}

