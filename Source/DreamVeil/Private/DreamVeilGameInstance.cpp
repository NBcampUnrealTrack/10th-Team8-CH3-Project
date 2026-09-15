#include "DreamVeilGameInstance.h"

#include "DispatchTableComponent.h"

//플레이어의 증강 기록을 저장
void UDreamVeilGameInstance::SavePlayerAugments(UDispatchTableComponent* PlayerDispatchTable)
{
    if (!PlayerDispatchTable)
    {
        return;
    }

    SavedPlayerAugmentHistory = PlayerDispatchTable->GetAugmentHistory();
}

//저장한 증강 기록을 새 레벨의 플레이어에게 다시 적용
bool UDreamVeilGameInstance::RestorePlayerAugments(UDispatchTableComponent* PlayerDispatchTable)
{
    if (!PlayerDispatchTable)
    {
        return false;
    }

    //첫 레벨이거나 재시작 직후라 저장한 기록이 없음
    if (SavedPlayerAugmentHistory.Num() == 0)
    {
        return true;
    }

    return PlayerDispatchTable->RestoreAugments(SavedPlayerAugmentHistory);
}

//저장한 증강 기록을 비움
void UDreamVeilGameInstance::ClearPlayerAugments()
{
    SavedPlayerAugmentHistory.Empty();
}
