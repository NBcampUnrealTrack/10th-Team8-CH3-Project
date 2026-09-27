#pragma once
#include "Commandlets/Commandlet.h"
#include "DreamVeilUIUpdateCommandlet.generated.h"

// Project editor module, not a plugin. Only -Apply and -Finalize save assets.
UCLASS()
class UDreamVeilUIUpdateCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UDreamVeilUIUpdateCommandlet();
    virtual int32 Main(const FString& Params) override;
};
