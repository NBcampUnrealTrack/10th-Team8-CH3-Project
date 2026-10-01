#include "DreamVeilAnimTools.h"

#include "Animation/AnimMontage.h"

//몽타주의 첫 슬롯 이름을 바꿈
bool UDreamVeilAnimTools::SetMontageSlotName(UAnimMontage* Montage, FName SlotName)
{
	//슬롯이 하나도 없는 몽타주는 애니메이션이 안 들어 있는 빈 몽타주라 이름만 바꿔도 소용이 없음
	if (!Montage || Montage->SlotAnimTracks.Num() == 0 || SlotName.IsNone())
	{
		return false;
	}

	//Modify를 먼저 부르는 이유 되돌리기 기록을 남기고 패키지를 바뀐 것으로 표시함
	Montage->Modify();

	Montage->SlotAnimTracks[0].SlotName = SlotName;

	Montage->PostEditChange();

	return true;
}

//몽타주의 첫 슬롯 이름
FName UDreamVeilAnimTools::GetMontageSlotName(const UAnimMontage* Montage)
{
	if (!Montage || Montage->SlotAnimTracks.Num() == 0)
	{
		return NAME_None;
	}

	return Montage->SlotAnimTracks[0].SlotName;
}
