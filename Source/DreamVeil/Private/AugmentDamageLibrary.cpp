// Fill out your copyright notice in the Description page of Project Settings.


#include "AugmentDamageLibrary.h"

#include "CombatStatsComponent.h"
#include "DispatchTableComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MonsterBase.h"

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

    //체력과 방어력은 스탯 컴포넌트가 들고 있음 없으면 데미지를 받을 수 없는 액터
    UCombatStatsComponent* DamagedStats = DamagedActor->FindComponentByClass<UCombatStatsComponent>();

    if (!DamagedStats)
    {
        return 0.0f;
    }

    //방어력 차감 후 체력 적용 이미 죽었거나 데미지가 없으면 0이 돌아옴 독 데미지는 방어력을 무시함
    const float FinalDamage = DamagedStats->ApplyIncomingDamage(Damage, IsPoisonDamage(DamageTypeClass));

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

    //독 데미지는 틱마다 흡혈이나 가시 갑옷 반사가 걸리지 않게 여기서 끝냄
    if (IsPoisonDamage(DamageTypeClass))
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

    //흡혈과 가시 갑옷은 증강이라 디스패치 테이블에게 물음 증강이 없는 액터면 건너뜀
    //때린 쪽은 실제로 얼마가 깎였는지 모르기 때문에 여기서 흡혈을 대신 걸어줌
    UDispatchTableComponent* AttackerTable = Attacker->FindComponentByClass<UDispatchTableComponent>();

    if (AttackerTable)
    {
        AttackerTable->ProcessOnDamageDealt(FinalDamage);
    }

    //가시 갑옷 반사 가시 갑옷이 없으면 0이라 ApplyThornReflectDamage 안에서 걸러짐
    UDispatchTableComponent* DamagedTable = DamagedActor->FindComponentByClass<UDispatchTableComponent>();

    if (DamagedTable)
    {
        ApplyThornReflectDamage(
            DamagedActor,
            Attacker,
            DamagedTable->CalculateThornReflectDamage(FinalDamage)
        );
    }

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
    //방어력 차감 체력 적용 흡혈 가시 갑옷은 받는 쪽 캐릭터 TakeDamage가 ProcessIncomingDamage로 처리
    //반환값은 받는 쪽 TakeDamage의 반환값 ProcessIncomingDamage 결과를 돌려주면 방어력을 뺀 데미지
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

//지속 공격 독 데미지를 보냄 독 표식이 붙어서 흡혈과 가시 갑옷 반사가 일어나지 않음
float UAugmentDamageLibrary::ApplyPoisonDamage(AActor* DamageCauser, AActor* Target, float Damage)
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
        FindEventInstigator(DamageCauser),
        DamageCauser,
        UPoisonDamageType::StaticClass()
    );
}

//독으로 들어온 데미지인지 확인
bool UAugmentDamageLibrary::IsPoisonDamage(TSubclassOf<UDamageType> DamageTypeClass)
{
    if (!DamageTypeClass)
    {
        return false;
    }

    return DamageTypeClass->IsChildOf(UPoisonDamageType::StaticClass());
}

//공격자의 현재 공격력을 가져옴 평타 데미지를 만들 때 씀
float UAugmentDamageLibrary::GetOutgoingDamage(AActor* DamageCauser)
{
    if (!DamageCauser)
    {
        return 0.0f;
    }

    UCombatStatsComponent* CauserStats = DamageCauser->FindComponentByClass<UCombatStatsComponent>();

    if (!CauserStats)
    {
        return 0.0f;
    }

    return CauserStats->CalculateOutgoingDamage();
}

//총이 무언가를 맞혔을 때 맞은 대상에게 데미지를 보내고 쏜 사람의 적중 증강을 발동
float UAugmentDamageLibrary::ApplyWeaponHit(AActor* DamageCauser, const FHitResult& HitResult, float Damage)
{
    if (!DamageCauser)
    {
        return 0.0f;
    }

    //맞은 대상에게 먼저 데미지 벽이나 바닥을 맞혔으면 대상이 없어서 0
    const float AppliedDamage = ApplyAugmentDamageToTarget(DamageCauser, HitResult.GetActor(), Damage);

    //투사체가 맞혔으면 쏜 사람을 찾아서 그 사람의 증강을 발동
    AActor* Shooter = FindAttacker(FindEventInstigator(DamageCauser), DamageCauser);

    if (!Shooter)
    {
        return AppliedDamage;
    }

    UDispatchTableComponent* ShooterTable = Shooter->FindComponentByClass<UDispatchTableComponent>();

    if (!ShooterTable)
    {
        return AppliedDamage;
    }

    //벽을 맞혀도 적중 지점 주변에 범위 공격이 터지도록 대상 유무와 상관없이 부름
    ShooterTable->ProcessWeaponHit(HitResult, Damage);

    return AppliedDamage;
}

//지정한 범위 안의 대상을 찾음 자기 자신은 제외
void UAugmentDamageLibrary::FindTargetsInRadius(AActor* OwnerActor, float Radius, TArray<AActor*>& OutTargets)
{
    OutTargets.Empty();

    if (!OwnerActor)
    {
        return;
    }

    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(OwnerActor);

    FindTargetsAtLocation(OwnerActor, OwnerActor->GetActorLocation(), Radius, ActorsToIgnore, OutTargets);
}

//원하는 위치 기준 범위 안의 대상을 찾음 ActorsToIgnore에 넣은 대상은 제외
void UAugmentDamageLibrary::FindTargetsAtLocation(UObject* WorldContextObject, FVector Location, float Radius, const TArray<AActor*>& ActorsToIgnore, TArray<AActor*>& OutTargets)
{
    OutTargets.Empty();

    if (!WorldContextObject)
    {
        return;
    }

    TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));

    //폰 전체를 잡음 아군 적군 구분이 필요하면 받은 쪽에서 IsEnemy로 거를 것
    UKismetSystemLibrary::SphereOverlapActors(
        WorldContextObject,
        Location,
        Radius,
        ObjectTypes,
        AActor::StaticClass(),
        ActorsToIgnore,
        OutTargets
    );
}

//두 액터가 서로 적인지 확인
bool UAugmentDamageLibrary::IsEnemy(AActor* ActorA, AActor* ActorB)
{
    if (!ActorA || !ActorB)
    {
        return false;
    }

    if (ActorA == ActorB)
    {
        return false;
    }

    //몬스터끼리는 아군 한쪽만 몬스터면 적 팀이 더 생기면 여기만 바꾸면 됨
    return ActorA->IsA<AMonsterBase>() != ActorB->IsA<AMonsterBase>();
}
