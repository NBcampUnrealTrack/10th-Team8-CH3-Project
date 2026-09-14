#include "AugmentSkillBase.h"

#include "DispatchTableComponent.h"
#include "GameFramework/Actor.h"

//증강을 얻을 때마다 불림
void UAugmentSkillBase::Apply()
{
}

//주인 체력이 바뀐 뒤 불림
void UAugmentSkillBase::OnHealthChanged(float OldValue, float NewValue)
{
}

//주인이 데미지를 입힌 뒤 불림
void UAugmentSkillBase::OnDamageDealt(float FinalDamage)
{
}

//주인이 데미지를 받았을 때 반사할 데미지 기본 0
float UAugmentSkillBase::CalculateReflectDamage(float FinalDamage)
{
    return 0.0f;
}

//주인 컴포넌트가 사라질 때 정리
void UAugmentSkillBase::Deactivate()
{
}

//타이머를 쓸 수 있도록 주인 컴포넌트의 월드를 돌려줌
UWorld* UAugmentSkillBase::GetWorld() const
{
    //클래스 기본 객체는 월드가 없음
    if (HasAnyFlags(RF_ClassDefaultObject))
    {
        return nullptr;
    }

    UDispatchTableComponent* OwnerComponent = GetOwnerComponent();

    if (!OwnerComponent)
    {
        return nullptr;
    }

    return OwnerComponent->GetWorld();
}

//이 스킬을 들고 있는 컴포넌트 NewObject로 만들 때 Outer로 넣어줌
UDispatchTableComponent* UAugmentSkillBase::GetOwnerComponent() const
{
    return GetTypedOuter<UDispatchTableComponent>();
}

//컴포넌트가 붙어 있는 액터
AActor* UAugmentSkillBase::GetOwnerActor() const
{
    UDispatchTableComponent* OwnerComponent = GetOwnerComponent();

    if (!OwnerComponent)
    {
        return nullptr;
    }

    return OwnerComponent->GetOwner();
}
