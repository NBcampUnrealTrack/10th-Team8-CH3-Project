#pragma once

#include "CoreMinimal.h"
#include "WeaponBase.h"
#include "RifleWeapon.generated.h"

//소총 기본 무기(권총)를 물려받아 데미지는 낮추고 연사력은 높임
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DREAMVEIL_API URifleWeapon : public UWeaponBase
{
	GENERATED_BODY()

public:

	URifleWeapon();
};
