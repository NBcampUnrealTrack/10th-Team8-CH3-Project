// Fill out your copyright notice in the Description page of Project Settings.


#include "DispatchTableForPlayerAndBossComponent.h"

#include "ContinuousAttackSkillComponent.h"
#include "PassiveSkillsComponent.h"
#include "SlowOpponentSkillComponent.h"
#include "WideRangeAttackSkillComponent.h"

//생성자
UDispatchTableForPlayerAndBossComponent::UDispatchTableForPlayerAndBossComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    //Has-A 2계층 컴포넌트들
    PassiveSkillsComponent = CreateDefaultSubobject<UPassiveSkillsComponent>(TEXT("PassiveSkillsComponent"));
    SlowOpponentSkillComponent = CreateDefaultSubobject<USlowOpponentSkillComponent>(TEXT("SlowOpponentSkillComponent"));
    WideRangeAttackSkillComponent = CreateDefaultSubobject<UWideRangeAttackSkillComponent>(TEXT("WideRangeAttackSkillComponent"));
    ContinuousAttackSkillComponent = CreateDefaultSubobject<UContinuousAttackSkillComponent>(TEXT("ContinuousAttackSkillComponent"));

    //BeginPlay 순서에 상관없이 쓸 수 있도록 여기서 등록
    RegisterAugmentFunctions();
}

//증강 효과 함수들을 테이블에 등록
void UDispatchTableForPlayerAndBossComponent::RegisterAugmentFunctions()
{
    //패시브
    AugmentMap.Add(EAugmentID::AttackUp, [this]()
        {
            if (PassiveSkillsComponent)
            {
                PassiveSkillsComponent->ExecuteAttackPowerUp();
            }
        });

    AugmentMap.Add(EAugmentID::DefenceUp, [this]()
        {
            if (PassiveSkillsComponent)
            {
                PassiveSkillsComponent->ExecuteDefencePowerUp();
            }
        });

    AugmentMap.Add(EAugmentID::HealthUp, [this]()
        {
            if (PassiveSkillsComponent)
            {
                PassiveSkillsComponent->ExecuteHealthUp();
            }
        });

    AugmentMap.Add(EAugmentID::Berserker, [this]()
        {
            if (PassiveSkillsComponent)
            {
                PassiveSkillsComponent->ExecuteBerserker();
            }
        });

    AugmentMap.Add(EAugmentID::LastFortress, [this]()
        {
            if (PassiveSkillsComponent)
            {
                PassiveSkillsComponent->ExecuteLastFortress();
            }
        });

    AugmentMap.Add(EAugmentID::ThornArmor, [this]()
        {
            if (PassiveSkillsComponent)
            {
                PassiveSkillsComponent->ExecuteThornArmor();
            }
        });

    AugmentMap.Add(EAugmentID::Vampire, [this]()
        {
            if (PassiveSkillsComponent)
            {
                PassiveSkillsComponent->ExecuteVampire();
            }
        });

    AugmentMap.Add(EAugmentID::Regeneration, [this]()
        {
            if (PassiveSkillsComponent)
            {
                PassiveSkillsComponent->ExecuteRegeneration();
            }
        });

    //패시브 이외 증강들
    AugmentMap.Add(EAugmentID::SlowEnemy, [this]()
        {
            if (SlowOpponentSkillComponent)
            {
                SlowOpponentSkillComponent->ExecuteSlowOpponent();
            }
        });

    AugmentMap.Add(EAugmentID::AreaAttack, [this]()
        {
            if (WideRangeAttackSkillComponent)
            {
                WideRangeAttackSkillComponent->ExecuteWideRangeAttack();
            }
        });

    AugmentMap.Add(EAugmentID::ContinuousAttack, [this]()
        {
            if (ContinuousAttackSkillComponent)
            {
                ContinuousAttackSkillComponent->ExecuteContinuousAttack();
            }
        });

    //무기 증강이 생기면 여기에 한 줄씩 추가하면 됨
}

//번호로 증강 효과를 실행
void UDispatchTableForPlayerAndBossComponent::ExecuteAugment(EAugmentID AugmentID)
{
    TFunction<void()>* FoundFunction = AugmentMap.Find(AugmentID);

    if (!FoundFunction)
    {
        return;
    }

    (*FoundFunction)();
}

//테이블에 등록된 증강인지 확인
bool UDispatchTableForPlayerAndBossComponent::HasAugment(EAugmentID AugmentID)
{
    return AugmentMap.Contains(AugmentID);
}

//패시브 스킬 Getter
UPassiveSkillsComponent* UDispatchTableForPlayerAndBossComponent::GetPassiveSkillsComponent()
{
    return PassiveSkillsComponent;
}
