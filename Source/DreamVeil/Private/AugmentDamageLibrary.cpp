// Fill out your copyright notice in the Description page of Project Settings.


#include "AugmentDamageLibrary.h"

#include "PassiveSkillsComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

//데미지를 일으킨 컨트롤러를 찾음 킬 판정이나 어그로에 쓰라고 같이 넘김
AController* UAugmentDamageLibrary::FindEventInstigator(AActor* DamageCauser)
{
    if (!DamageCauser)
    {
        return nullptr;
    }

    APawn* CauserPawn = Cast<APawn>(DamageCauser);

    if (CauserPawn)
    {
        return CauserPawn->GetController();
    }

    //무기나 투사체처럼 폰이 아닌 것이 때린 경우
    return DamageCauser->GetInstigatorController();
}

//한 대상에게 데미지를 보냄 내부에서 UGameplayStatics::ApplyDamage를 부름
float UAugmentDamageLibrary::ApplyAugmentDamageToTarget(AActor* DamageCauser, AActor* Target, float Damage)
{
    if (!Target)
    {
        return 0.0f;
    }

    if (Damage <= 0.0f)
    {
        return 0.0f;
    }

    //여기서 보낸 데미지는 대상의 TakeDamage를 거쳐 OnTakeAnyDamage로 퍼짐
    //방어력 차감 체력 적용 가시 갑옷 흡혈은 전부 받는 쪽 PassiveSkillsComponent가 처리
    return UGameplayStatics::ApplyDamage(
        Target,
        Damage,
        FindEventInstigator(DamageCauser),
        DamageCauser,
        UDamageType::StaticClass()
    );
}

//여러 대상에게 같은 데미지를 보냄
void UAugmentDamageLibrary::ApplyAugmentDamage(AActor* DamageCauser, const TArray<AActor*>& Targets, float Damage)
{
    for (AActor* Target : Targets)
    {
        ApplyAugmentDamageToTarget(DamageCauser, Target, Damage);
    }
}

//가시 갑옷 반사 데미지를 보냄 반사 표식이 붙어서 되받아치기가 일어나지 않음
float UAugmentDamageLibrary::ApplyThornReflectDamage(AActor* ThornOwner, AActor* Target, float Damage)
{
    if (!Target)
    {
        return 0.0f;
    }

    if (Damage <= 0.0f)
    {
        return 0.0f;
    }

    return UGameplayStatics::ApplyDamage(
        Target,
        Damage,
        FindEventInstigator(ThornOwner),
        ThornOwner,
        UThornReflectDamageType::StaticClass()
    );
}

//반사로 들어온 데미지인지 확인
bool UAugmentDamageLibrary::IsThornReflectDamage(const UDamageType* DamageType)
{
    if (!DamageType)
    {
        return false;
    }

    return DamageType->IsA(UThornReflectDamageType::StaticClass());
}

//공격자의 현재 공격력을 가져옴 평타 데미지를 만들 때 씀
float UAugmentDamageLibrary::GetOutgoingDamage(AActor* DamageCauser)
{
    if (!DamageCauser)
    {
        return 0.0f;
    }

    UPassiveSkillsComponent* CauserPassive = DamageCauser->FindComponentByClass<UPassiveSkillsComponent>();

    if (!CauserPassive)
    {
        return 0.0f;
    }

    return CauserPassive->CalculateOutgoingDamage();
}

//지정한 범위 안의 대상을 찾음 자기 자신은 제외
void UAugmentDamageLibrary::FindTargetsInRadius(AActor* OwnerActor, float Radius, TArray<AActor*>& OutTargets)
{
    OutTargets.Empty();

    if (!OwnerActor)
    {
        return;
    }

    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(OwnerActor);

    //지금은 폰 전체를 잡음 아군 적군 구분은 팀 태그가 정해지면 여기서 걸러냄
    UKismetSystemLibrary::SphereOverlapActors(
        OwnerActor,
        OwnerActor->GetActorLocation(),
        Radius,
        ObjectTypes,
        AActor::StaticClass(),
        ActorsToIgnore,
        OutTargets
    );
}
