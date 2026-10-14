#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DreamPoolable.generated.h"

UINTERFACE(MinimalAPI)
class UDreamPoolable : public UInterface { GENERATED_BODY() };

class DREAMVEIL_API IDreamPoolable
{
    GENERATED_BODY()
public:
    virtual void PrepareForPoolReuse() = 0;
    virtual void OnPoolAcquired() = 0;
    virtual void OnPoolReturned() {}
    virtual int32 GetPoolCapacity() const { return 32; }
    virtual float GetPoolReuseDelay() const { return 0.05f; }
    uint32 GetPoolGeneration() const { return PoolGeneration; }
    void AdvancePoolGeneration() { ++PoolGeneration; }
private:
    uint32 PoolGeneration = 0;
};
