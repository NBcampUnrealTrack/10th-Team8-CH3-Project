#pragma once
#include "CoreMinimal.h"
bool ApplyDreamVeilUIEdits(const FString& IconDirectory);
bool VerifyDreamVeilUIEdits(bool bFinalize);
bool RenderDreamVeilUI();
bool TestDreamVeilCamera();
bool UpdateHardCameraAssets(bool bApply);
bool FixDreamVeilShopAndInventory();
bool ImportDreamVeilBGM(const FString& SourceDirectory);
bool WireMainMenuBGM();
