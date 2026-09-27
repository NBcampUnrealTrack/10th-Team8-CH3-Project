#include "RifleWeapon.h"

//임시 수치 기획이 정해지면 바꿀 것
URifleWeapon::URifleWeapon()
{
	Damage = 5.0f;
	Range = 10000.0f;
	FireInterval = 0.3f;

	//소총은 누르고 있으면 계속 나감
	bAutomatic = true;

	//연사라 한 발당 반동을 권총보다 작게 둠 권총과 같게 두면 몇 초만 눌러도 하늘을 보게 됨
	//대신 발수가 많아서 쌓이는 양은 권총보다 큼
	RecoilPitch = 0.5f;
	RecoilYaw = 0.5f;
}

//소총이 끼울 수 있는 칸
TArray<EWeaponPartSlot> URifleWeapon::GetPartSlots() const
{
	//공용 3칸은 부모 것을 그대로 받아서 쓰고 소총 전용 2칸만 더함
	//공용 칸이 바뀌어도 부모 한 곳만 고치면 권총과 소총에 같이 반영됨
	TArray<EWeaponPartSlot> Slots = Super::GetPartSlots();
	Slots.Add(EWeaponPartSlot::Stock);
	Slots.Add(EWeaponPartSlot::Foregrip);

	return Slots;
}
