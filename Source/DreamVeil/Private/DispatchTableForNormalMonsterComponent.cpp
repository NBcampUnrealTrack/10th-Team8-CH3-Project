// Fill out your copyright notice in the Description page of Project Settings.


#include "DispatchTableForNormalMonsterComponent.h"

#include "PassiveSkillsComponent.h"

//생성자
UDispatchTableForNormalMonsterComponent::UDispatchTableForNormalMonsterComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    //Has-A 2계층 패시브 스킬
    PassiveSkillsComponent = CreateDefaultSubobject<UPassiveSkillsComponent>(TEXT("PassiveSkillsComponent"));

    //BeginPlay 순서에 상관없이 쓸 수 있도록 여기서 등록
    RegisterPassiveAugmentFunctions();
}

//증강 효과 함수들을 테이블에 등록
void UDispatchTableForNormalMonsterComponent::RegisterPassiveAugmentFunctions()
{
    PassiveAugmentMap.Add(EAugmentID::AttackUp, [this]()
        {
            if (PassiveSkillsComponent)
            {
                PassiveSkillsComponent->ExecuteAttackPowerUp();
            }
        });

    PassiveAugmentMap.Add(EAugmentID::DefenceUp, [this]()
        {
            if (PassiveSkillsComponent)
            {
                PassiveSkillsComponent->ExecuteDefencePowerUp();
            }
        });

    PassiveAugmentMap.Add(EAugmentID::HealthUp, [this]()
        {
            if (PassiveSkillsComponent)
            {
                PassiveSkillsComponent->ExecuteHealthUp();
            }
        });

    PassiveAugmentMap.Add(EAugmentID::Berserker, [this]()
        {
            if (PassiveSkillsComponent)
            {
                PassiveSkillsComponent->ExecuteBerserker();
            }
        });

    PassiveAugmentMap.Add(EAugmentID::ThornArmor, [this]()
        {
            if (PassiveSkillsComponent)
            {
                PassiveSkillsComponent->ExecuteThornArmor();
            }
        });

    PassiveAugmentMap.Add(EAugmentID::Vampire, [this]()
        {
            if (PassiveSkillsComponent)
            {
                PassiveSkillsComponent->ExecuteVampire();
            }
        });

    PassiveAugmentMap.Add(EAugmentID::Regeneration, [this]()
        {
            if (PassiveSkillsComponent)
            {
                PassiveSkillsComponent->ExecuteRegeneration();
            }
        });
}

//번호로 패시브 증강 효과를 실행
void UDispatchTableForNormalMonsterComponent::ExecuteAugment(EAugmentID AugmentID)
{
    TFunction<void()>* FoundFunction = PassiveAugmentMap.Find(AugmentID);

    if (!FoundFunction)
    {
        return;
    }

    (*FoundFunction)();
}

//정해진 증강 묶음을 한 번에 적용 타입별 고정 패시브용
void UDispatchTableForNormalMonsterComponent::ExecuteAugments(const TArray<EAugmentID>& AugmentIDs)
{
    for (EAugmentID AugmentID : AugmentIDs)
    {
        ExecuteAugment(AugmentID);
    }
}

//같은 증강을 여러 번 적용 엔드리스 웨이브 진행도에 따른 체공방 증가용
void UDispatchTableForNormalMonsterComponent::ExecuteAugmentRepeat(EAugmentID AugmentID, int32 RepeatCount)
{
    for (int32 Index = 0; Index < RepeatCount; ++Index)
    {
        ExecuteAugment(AugmentID);
    }
}

//테이블에 등록된 증강인지 확인
bool UDispatchTableForNormalMonsterComponent::HasAugment(EAugmentID AugmentID)
{
    return PassiveAugmentMap.Contains(AugmentID);
}

//패시브 스킬 Getter
UPassiveSkillsComponent* UDispatchTableForNormalMonsterComponent::GetPassiveSkillsComponent()
{
    return PassiveSkillsComponent;
}
