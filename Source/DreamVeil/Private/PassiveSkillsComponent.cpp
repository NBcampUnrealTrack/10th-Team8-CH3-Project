// Fill out your copyright notice in the Description page of Project Settings.


#include "PassiveSkillsComponent.h"

#include "AttackComponent.h"
#include "DefenceComponent.h"
#include "HealthComponent.h"


// 생성자
UPassiveSkillsComponent::UPassiveSkillsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
    InitializeBasicStatsComponents();
    bBerserker = false;
    bVampire = false;
    bDefencePowerUp = false;
    bThornArmor = false;
    bRegeneration = false;
    bAttackPowerUp = false;
}
// 전투 컴포넌트 초기화
void UPassiveSkillsComponent::InitializeBasicStatsComponents()
{
    AttackComponent = NewObject<UAttackComponent>(GetOwner());
    AttackComponent->RegisterComponent();

    DefenceComponent = NewObject<UDefenceComponent>(GetOwner());
    DefenceComponent->RegisterComponent();

    HealthComponent = NewObject<UHealthComponent>(GetOwner());
    HealthComponent->RegisterComponent();
}




