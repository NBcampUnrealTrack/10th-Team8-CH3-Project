// Fill out your copyright notice in the Description page of Project Settings.


#include "AugmentDamageLibrary.h"

#include "DispatchTableComponent.h"
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

//흡혈과 반사를 받을 실제 공격자를 찾음 투사체가 때렸으면 쏜 폰을 돌려줌
AActor* UAugmentDamageLibrary::FindAttacker(AController* EventInstigator, AActor* DamageCauser)
{
    //컨트롤러가 조종 중인 폰이 가장 확실함
    if (EventInstigator && EventInstigator->GetPawn())
    {
        return EventInstigator->GetPawn();
    }

    if (!DamageCauser)
    {
        return nullptr;
    }

    //컨트롤러가 없는 폰이 직접 때린 경우
    if (DamageCauser->IsA<APawn>())
    {
        return DamageCauser;
    }

    //투사체는 스폰할 때 넣어준 Instigator가 쏜 폰
    return DamageCauser->GetInstigator();
}

//받은 데미지를 순서대로 처리 방어력 차감 -> 체력 적용 -> 흡혈 -> 가시 갑옷 반사
float UAugmentDamageLibrary::ProcessIncomingDamage(AActor* DamagedActor, float Damage, TSubclassOf<UDamageType> DamageTypeClass, AController* EventInstigator, AActor* DamageCauser)
{
    if (!DamagedActor)
    {
        return 0.0f;
    }

    UDispatchTableComponent* DamagedTable = DamagedActor->FindComponentByClass<UDispatchTableComponent>();

    if (!DamagedTable)
    {
        return 0.0f;
    }

    //방어력 차감 후 체력 적용 이미 죽었거나 데미지가 없으면 0이 돌아옴
    const float FinalDamage = DamagedTable->ApplyIncomingDamage(Damage);

    if (FinalDamage <= 0.0f)
    {
        return 0.0f;
    }

    //반사로 들어온 데미지는 다시 반사하지 않고 상대를 회복시키지도 않음
    //서로 가시 갑옷을 들고 있을 때 무한히 주고받는 것을 막음
    if (IsThornReflectDamage(DamageTypeClass))
    {
        return FinalDamage;
    }

    AActor* Attacker = FindAttacker(EventInstigator, DamageCauser);

    if (!Attacker)
    {
        return FinalDamage;
    }

    //자기가 자기를 때린 경우는 흡혈도 반사도 없음
    if (Attacker == DamagedActor)
    {
        return FinalDamage;
    }

    //때린 쪽은 실제로 얼마가 깎였는지 모르기 때문에 여기서 흡혈을 대신 걸어줌
    UDispatchTableComponent* AttackerTable = Attacker->FindComponentByClass<UDispatchTableComponent>();

    if (AttackerTable)
    {
        AttackerTable->ProcessOnDamageDealt(FinalDamage);
    }

    //가시 갑옷 반사 가시 갑옷이 없으면 0이라 ApplyThornReflectDamage 안에서 걸러짐
    ApplyThornReflectDamage(
        DamagedActor,
        Attacker,
        DamagedTable->CalculateThornReflectDamage(FinalDamage)
    );

    return FinalDamage;
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

    //여기서 보낸 데미지는 대상의 TakeDamage로 들어감
    //방어력 차감 체력 적용 흡혈 가시 갑옷은 받는 쪽 5계층 TakeDamage가 ProcessIncomingDamage로 처리
    //반환값은 받는 쪽 TakeDamage의 반환값 5계층이 ProcessIncomingDamage 결과를 돌려주면 방어력을 뺀 데미지
    //남은 체력보다 커도 자르지 않음 흡혈과 가시 갑옷도 이 값 기준(오버킬 허용)
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
bool UAugmentDamageLibrary::IsThornReflectDamage(TSubclassOf<UDamageType> DamageTypeClass)
{
    if (!DamageTypeClass)
    {
        return false;
    }

    return DamageTypeClass->IsChildOf(UThornReflectDamageType::StaticClass());
}

//공격자의 현재 공격력을 가져옴 평타 데미지를 만들 때 씀
float UAugmentDamageLibrary::GetOutgoingDamage(AActor* DamageCauser)
{
    if (!DamageCauser)
    {
        return 0.0f;
    }

    UDispatchTableComponent* CauserTable = DamageCauser->FindComponentByClass<UDispatchTableComponent>();

    if (!CauserTable)
    {
        return 0.0f;
    }

    return CauserTable->CalculateOutgoingDamage();
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
