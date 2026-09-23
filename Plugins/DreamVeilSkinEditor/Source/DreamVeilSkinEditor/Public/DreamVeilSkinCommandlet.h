#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "DreamVeilSkinCommandlet.generated.h"

UCLASS()
class DREAMVEILSKINEDITOR_API UDreamVeilSkinCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UDreamVeilSkinCommandlet();
    virtual int32 Main(const FString& Params) override;
};
