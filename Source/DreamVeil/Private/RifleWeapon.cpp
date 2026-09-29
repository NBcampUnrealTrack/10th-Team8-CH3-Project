#include "RifleWeapon.h"

//임시 수치 기획이 정해지면 바꿀 것
URifleWeapon::URifleWeapon()
{
	//공격력이 무기 데미지에 더해지는 구조라 무기 기본값이 낮으면 공격력에 묻힘
	//플레이어 공격력 10 기준 5/0.3이면 초당 50 권총은 57이라 소총이 더 약했음
	//8/0.25로 올려서 초당 72 권총보다 26퍼센트 높게 둠 대신 한 발과 헤드샷은 권총이 더 셈
	Damage = 8.0f;
	Range = 10000.0f;
	FireInterval = 0.25f;

	//소총은 누르고 있으면 계속 나감
	bAutomatic = true;

	//연사라 한 발당 반동을 권총보다 작게 둠 권총과 같게 두면 몇 초만 눌러도 하늘을 보게 됨
	//대신 발수가 많아서 쌓이는 양은 권총보다 큼
	RecoilPitch = 0.5f;
	RecoilYaw = 0.5f;

	//연사라 권총보다 훨씬 자주 울려서 원본의 절반으로 낮춤
	// 더낮춤 진짜 엄청 시끄러워서 죽는줄알았네
	FireSoundVolume = 0.2f;
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
