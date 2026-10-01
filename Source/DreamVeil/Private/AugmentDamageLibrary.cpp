    //방어력 차감 후 체력 적용 이미 죽었거나 데미지가 없으면 0이 돌아옴 독 데미지는 방어력을 무시함
    //독이나 가시 갑옷 반사로 죽어도 보상이 나가야 해서 아래 조기 반환들보다 먼저 확인
    //독 데미지는 틱마다 흡혈이나 가시 갑옷 반사가 걸리지 않게 여기서 끝냄
//지속 공격 독 데미지를 보냄 독 표식이 붙어서 흡혈과 가시 갑옷 반사가 일어나지 않음
//독으로 들어온 데미지인지 확인
 // Fill out your copyright notice in the Description page of Project Settings.


#include "AugmentDamageLibrary.h"
#include "MonsterProgressionLibrary.h"

#include "CombatStatsComponent.h"
#include "MonsterCollision.h"
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

    //화염 데미지에는 두 가지 규칙이 걸림 방어력을 무시하고 흡혈과 가시 갑옷 반사도 일으키지 않음
    //두 규칙이 같은 판정을 쓰므로 한 번만 물어보고 아래에서 같이 씀
    const bool bIsFireDamage = IsFireDamage(DamageTypeClass);

    //방어력 차감 후 체력 적용 이미 죽었거나 데미지가 없으면 0이 돌아옴
    const float FinalDamage = DamagedStats->ApplyIncomingDamage(Damage, bIsFireDamage);

    if (FinalDamage <= 0.0f)
    {
        return 0.0f;
    }

    //이번 데미지로 죽었으면 잡은 쪽에게 처치 보상(꿈의 조각 확률 파츠)
    //이미 죽어 있던 대상은 위에서 0이 돌아와 여기까지 못 오므로 여기서 죽어 있으면 이번 한 방에 죽은 것
    //화염이나 가시 갑옷 반사로 죽어도 보상이 나가야 해서 아래 조기 반환들보다 먼저 확인
    if (DamagedStats->IsDead())
    {
        AActor* Killer = FindAttacker(EventInstigator, DamageCauser);
        UInventoryComponent* KillerInventory = Killer ? Killer->FindComponentByClass<UInventoryComponent>() : nullptr;

        //인벤토리가 있는 쪽(플레이어)이 잡았을 때만 보상 몬스터끼리 죽인 경우는 없음
        if (KillerInventory)
        {
            KillerInventory->ReceiveKillRewards(DamagedActor);
        }

        //경험치도 같은 자리에서 줌 꿈의 조각과 판정이 갈라지면 조각은 받았는데 경험치는 안 들어오는 일이 생김
        //등급별 값과 레벨 보정은 라이브러리가 정함 여기서는 누가 무엇을 잡았는지만 넘김
        UMonsterProgressionLibrary::GrantKillExperience(Killer, DamagedActor);

        //잡은 만큼 잠식도를 내림 경험치와 같은 자리에 두어서 판정이 갈라지지 않게 함
        //잡은 쪽을 넘기지 않는 이유 화염이나 가시 반사로 죽어도 플레이어가 잡은 것이라 잠식도는 내려가야 함
        UMonsterProgressionLibrary::GrantKillCorruptionRelief(DamagedActor);
    }

    //반사로 들어온 데미지는 다시 반사하지 않고 상대를 회복시키지도 않음
    //서로 가시 갑옷을 들고 있을 때 무한히 주고받는 것을 막음
    if (IsThornReflectDamage(DamageTypeClass))
    {
        //가시 연출은 ApplyThornReflectDamage가 냄 여기서 내지 않는 이유
        //연출 에셋을 가시 갑옷 주인(플레이어)이 들고 있어서 맞은 쪽 컴포넌트로는 꺼낼 수 없음
        return FinalDamage;
    }

    //화염 데미지는 틱마다 흡혈이나 가시 갑옷 반사가 걸리지 않게 여기서 끝냄
    if (bIsFireDamage)
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

    //가시 갑옷 반사 가시 갑옷이 없거나 방어력이 0이면 0이라 ApplyThornReflectDamage 안에서 걸러짐
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
    //조기 반환보다 먼저 찍음 반사량이 0인 채로 끝나는 경우를 봐야 해서
    //반사량 0은 가시 갑옷이 없거나 가시 갑옷은 있는데 방어력이 0이라는 뜻 둘을 구분하려면 방어력도 같이 확인할 것
    if (THORN_ARMOR_DRAW_DEBUG)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ThornArmor] 주인 %s -> 대상 %s 반사량 %.1f"),
            *GetNameSafe(ThornOwner), *GetNameSafe(Target), Damage);
    }

    if (!Target)
    {
        return 0.0f;
    }

    if (Damage <= 0.0f)
    {
        return 0.0f;
    }

    const float ReflectedDamage = UGameplayStatics::ApplyDamage(
        Target,
        Damage,
        FindEventInstigator(ThornOwner),
        ThornOwner,
        UThornReflectDamageType::StaticClass()
    );

    //실제로 들어갔을 때만 가시를 냄 이미 죽어 있던 대상이면 0이 돌아와서 헛방이 안 보임
    if (ReflectedDamage > 0.0f)
    {
        //연출 에셋은 가시 갑옷을 가진 쪽이 들고 있고 가시는 맞은 쪽 발밑에서 솟음
        //맞은 쪽이 들고 있게 하면 몬스터 블루프린트마다 같은 에셋을 넣어야 해서 관리가 안 됨
        //연출 에셋은 가시 갑옷을 가진 쪽의 증강 컴포넌트가 들고 있음
        //맞은 쪽이 들고 있게 하면 몬스터 블루프린트마다 같은 에셋을 넣어야 해서 관리가 안 됨
        UDispatchTableComponent* ThornOwnerTable = ThornOwner ? ThornOwner->FindComponentByClass<UDispatchTableComponent>() : nullptr;

        if (THORN_ARMOR_DRAW_DEBUG)
        {
            UE_LOG(LogTemp, Warning, TEXT("[ThornArmor] 실제 들어간 데미지 %.1f 주인 증강 테이블 %s"),
                ReflectedDamage, ThornOwnerTable ? TEXT("있음") : TEXT("없음"));
        }

        if (ThornOwnerTable)
        {
            ThornOwnerTable->PlayThornReflectEffect(Target);
        }
    }

    return ReflectedDamage;
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

//지속 공격 화염 데미지를 보냄 화염 표식이 붙어서 흡혈과 가시 갑옷 반사가 일어나지 않음
float UAugmentDamageLibrary::ApplyFireDamage(AActor* DamageCauser, AActor* Target, float Damage)
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
        UFireDamageType::StaticClass()
    );
}

//화염으로 들어온 데미지인지 확인
bool UAugmentDamageLibrary::IsFireDamage(TSubclassOf<UDamageType> DamageTypeClass)
{
    if (!DamageTypeClass)
    {
        return false;
    }

    return DamageTypeClass->IsChildOf(UFireDamageType::StaticClass());
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
    //팀 판정을 먼저 하고 스탯 컴포넌트는 나중에 찾음 적을 맞혔을 때 컴포넌트 검색을 건너뛰려는 것
    //벽이나 바닥은 둘 다 몬스터가 아니라 팀 판정에서 아군으로 나오지만 스탯 컴포넌트가 없어서 그대로 진행됨
    if (Shooter && HitActor && !IsEnemy(Shooter, HitActor) && HitActor->FindComponentByClass<UCombatStatsComponent>())
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
    //여기(총 적중)에서만 배율을 거는 이유 범위 공격 화염 가시 갑옷 반사는 조준해서 맞히는 공격이 아니라 평타로 들어가야 함
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

    //플레이어는 Pawn이지만 몬스터는 전용 채널을 씀 Pawn만 넣으면 몬스터가 한 마리도 안 잡힘
    //MonsterBase가 이동 캡슐의 오브젝트 타입을 MonsterCollision::Monster로 바꿔둠
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
    ObjectTypes.Add(UEngineTypes::ConvertToObjectType(MonsterCollision::Monster));

    //양쪽을 다 잡음 아군 적군 구분이 필요하면 받은 쪽에서 IsEnemy로 거를 것
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
