#include "RifleWeapon.h"

//임시 수치 기획이 정해지면 바꿀 것
URifleWeapon::URifleWeapon()
{
	Damage = 5.0f;
	Range = 10000.0f;
	FireInterval = 0.3f;

	//소총은 누르고 있으면 계속 나감
	bAutomatic = true;
}
