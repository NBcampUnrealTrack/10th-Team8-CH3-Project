 // Fill out your copyright notice in the Description page of Project Settings.


#include "AugmentDamageLibrary.h"

#include "CombatStatsComponent.h"
#include "DispatchTableComponent.h"
#include "InventoryComponent.h"
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

    //이번 데미지로 죽었으면 잡은 쪽에게 처치 보상(꿈의 조각 확률 파츠)
    //이미 죽어 있던 대상은 위에서 0이 돌아와 여기까지 못 오므로 여기서 죽어 있으면 이번 한 방에 죽은 것
    //독이나 가시 갑옷 반사로 죽어도 보상이 나가야 해서 아래 조기 반환들보다 먼저 확인
    if (DamagedStats->IsDead())
    {
        AActor* Killer = FindAttacker(EventInstigator, DamageCauser);
        UInventoryComponent* KillerInventory = Killer ? Killer->FindComponentByClass<UInventoryComponent>() : nullptr;

        //인벤토리가 있는 쪽(플레이어)이 잡았을 때만 보상 몬스터끼리 죽인 경우는 없음
        if (KillerInventory)
        {
            KillerInventory->ReceiveKillRewards(DamagedActor);
        }
    }

    //반사로 들어온 데미지는 다시 반사하지 않고 상대를 회복시키지도 않음
    //서로 가시 갑옷을 들고 있을 때 무한히 주고받는 것을 막음
    if (IsThornReflectDamage(DamageTypeClass))
    {
        //반사를 맞은 쪽(보통 몬스터)의 몸에서 가시가 솟는 연출
        //여기서 재생하는 이유 반사가 실제로 들어간 순간이라 헛방이 없음
        DamagedStats->PlayThornReflectEffect();

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

    AActor* HitActor = HitResult.GetActor();

    //아무것도 맞지 않은 결과 빗나간 트레이스나 비어 있는 오버랩 결과면 범위 공격이 원점에서 터지지 않게 끝냄
    if (!HitResult.bBlockingHit && !HitActor)
    {
        return 0.0f;
    }

    //투사체가 맞혔으면 쏜 사람을 찾음 팀 구분과 적중 증강 발동에 씀
    AActor* Shooter = FindAttacker(FindEventInstigator(DamageCauser), DamageCauser);

    //아군이나 자기 자신을 맞혔으면 데미지도 적중 증강(범위 공격 감속 지속 공격)도 없음
    //벽이나 바닥처럼 스탯 컴포넌트가 없는 액터는 팀 구분 대상이 아니라서 그대로 진행
    if (Shooter && HitActor && HitActor->FindComponentByClass<UCombatStatsComponent>() && !IsEnemy(Shooter, HitActor))
    {
        return 0.0f;
    }

    //오버랩으로 들어온 적중은 충돌 지점이 비어 있을 수 있어서 맞은 대상 위치를 적중 지점으로 씀
    FHitResult WeaponHitResult = HitResult;

    if (!HitResult.bBlockingHit)
    {
        WeaponHitResult.ImpactPoint = HitActor->GetActorLocation();
        WeaponHitResult.Location = WeaponHitResult.ImpactPoint;
    }

    //머리를 맞혔으면 데미지를 올림 머리 판정은 몬스터가 자기 머리 구로 직접 함
    //여기(총 적중)에서만 배율을 거는 이유 범위 공격 독 가시 갑옷 반사는 조준해서 맞히는 공격이 아니라 평타로 들어가야 함
    const AMonsterBase* HitMonster = Cast<AMonsterBase>(HitActor);
    const float HeadshotDamage = (HitMonster && HitMonster->IsHeadshotHit(HitResult)) ? Damage * HEADSHOT_DAMAGE_MULTIPLIER : Damage;

    //맞은 대상에게 먼저 데미지 벽이나 바닥을 맞혔으면 스탯이 없어서 0
    const float AppliedDamage = ApplyAugmentDamageToTarget(DamageCauser, HitActor, HeadshotDamage);

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
    //헤드샷 배율을 뺀 원래 데미지를 넘김 범위 공격의 폭발 데미지까지 머리 배율을 타면 안 됨
    ShooterTable->ProcessWeaponHit(WeaponHitResult, Damage);

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
    //하나라도 없으면 판정할 수 없으니 적 아님
    if (!ActorA || !ActorB)
    {
        return false;
    }

    //자기 자신은 적 아님
    if (ActorA == ActorB)
    {
        return false;
    }

    //각자 몬스터인지 확인 팀이 더 생기면 여기부터 바꾸면 됨
    const bool bIsActorAMonster = ActorA->IsA<AMonsterBase>();
    const bool bIsActorBMonster = ActorB->IsA<AMonsterBase>();

    //둘 다 몬스터면 아군
    if (bIsActorAMonster && bIsActorBMonster)
    {
        return false;
    }

    //둘 다 몬스터가 아니면 플레이어끼리 아군
    if (!bIsActorAMonster && !bIsActorBMonster)
    {
        return false;
    }

    //한쪽만 몬스터면 적
    return true;
}
