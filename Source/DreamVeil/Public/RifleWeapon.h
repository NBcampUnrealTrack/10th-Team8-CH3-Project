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

	//공용 3칸에 소총 전용 2칸(개머리판 앞손잡이)을 더해 5칸
	//부모가 UFUNCTION으로 열어둔 함수라 override 쪽에는 UFUNCTION을 다시 붙이지 않음
	virtual TArray<EWeaponPartSlot> GetPartSlots() const override;
};
