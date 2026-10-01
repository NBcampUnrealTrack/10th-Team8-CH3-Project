#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DreamVeilAnimTools.generated.h"

class UAnimMontage;

//에디터에서만 쓰는 애니메이션 에셋 손질 도구
//따로 만든 이유 파이썬에서 몽타주의 슬롯 이름을 건드릴 방법이 없음(SlotAnimTracks가 파이썬에 안 열려 있음)
//슬롯 이름이 그 몬스터 애님 블루프린트의 Slot 노드 이름과 다르면
//Montage_Play는 성공해서 길이까지 돌려주는데 화면에는 아무것도 안 나옴 그래서 눈으로 찾기 어려운 사고가 남
//몬스터마다 슬롯 이름이 다름 Stickman과 TwinBlast는 DefaultSlot, Rampage는 UpperBody, Sevarog는 FullBody
UCLASS()
class UDreamVeilAnimTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//몽타주가 쓰는 첫 슬롯의 이름을 바꿈 저장은 부른 쪽에서 할 것
	UFUNCTION(BlueprintCallable, Category = "DreamVeil|Anim")
	static bool SetMontageSlotName(UAnimMontage* Montage, FName SlotName);

	//몽타주가 지금 쓰는 첫 슬롯 이름 슬롯이 없으면 None
	UFUNCTION(BlueprintPure, Category = "DreamVeil|Anim")
	static FName GetMontageSlotName(const UAnimMontage* Montage);
};
