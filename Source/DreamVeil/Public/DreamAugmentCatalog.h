#pragma once
#include "CoreMinimal.h"
#include "AugmentTypes.h"

struct DREAMVEIL_API FDreamAugmentDefinition
{
    EAugmentID ID;
    FString Code,Name,Description,CardText;
    EAugmentCategory Category;
    bool Evolution=false,Permanent=false;
    TArray<EAugmentID> Ingredients;
    float Cost=0,Cooldown=0;
    EAugmentID Icon=EAugmentID::AttackUp;
};
namespace DreamAugmentCatalog
{
    DREAMVEIL_API const TArray<FDreamAugmentDefinition>& All();
    DREAMVEIL_API const FDreamAugmentDefinition* Find(EAugmentID ID);
    DREAMVEIL_API EAugmentID FromCode(const FString& Code);
}
