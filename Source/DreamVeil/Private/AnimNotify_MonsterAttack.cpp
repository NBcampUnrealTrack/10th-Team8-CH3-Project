// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify_MonsterAttack.h"
#include "MonsterBase.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_MonsterAttack::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	// Mesh에서 액터 뺀 다음 ABaseMonster으로 캐스팅
	if (AMonsterBase* Monster = Cast<AMonsterBase>(MeshComp->GetOwner()))
	{
		Monster->ExecuteAttack();
	}
}
