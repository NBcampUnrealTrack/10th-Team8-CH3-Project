#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "DreamAugmentCatalog.h"
#include "DreamUIBlueprintLibrary.h"
#include "Engine/Texture2D.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDreamIconCoverage,"DreamVeil.UI.EachAugmentHasItsOwnCompactIcon",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDreamIconCoverage::RunTest(const FString& Parameters)
{
    auto* Set=LoadObject<UDreamAugmentIconSet>(nullptr,TEXT("/Game/UI/AugmentIcons/DA_AugmentIcons.DA_AugmentIcons"));
    if(!TestNotNull(TEXT("Icon set loads"),Set))return false;
    TestEqual(TEXT("All current augment definitions have icon entries"),Set->Icons.Num(),DreamAugmentCatalog::All().Num());
    TSet<UTexture2D*> Unique;
    for(const auto& Def:DreamAugmentCatalog::All())
    {
        const auto* Found=Set->Icons.Find(Def.ID);UTexture2D* Texture=Found?Found->Get():nullptr;
        if(!TestNotNull(*Def.Code,Texture))continue;
        TestFalse(*(Def.Code+TEXT(" has unique artwork")),Unique.Contains(Texture));Unique.Add(Texture);
        TestTrue(*(Def.Code+TEXT(" uses UI LOD")),Texture->LODGroup==TEXTUREGROUP_UI);
        TestTrue(*(Def.Code+TEXT(" source is at most 256px")),Texture->Source.GetSizeX()<=256&&Texture->Source.GetSizeY()<=256);
        TestTrue(*(Def.Code+TEXT(" does not stream mid-combat")),Texture->NeverStream!=0);
    }
    return true;
}
#endif
