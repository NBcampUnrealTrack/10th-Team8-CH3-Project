#include "DreamVeilGameInstance.h"

#include "DispatchTableComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

//로비 맵 경로 메인 메뉴에서 들어오고 레벨을 깰 때마다 돌아오는 곳
const TCHAR* const LOBBY_MAP_PATH = TEXT("/Game/Maps/Level/Lobby");

//진행 순서대로 늘어놓은 레벨 맵 경로 0번이 L1
//짧은 이름 대신 전체 경로를 쓰는 이유 이름만 쓰면 같은 이름의 맵이 생겼을 때 엉뚱한 맵이 열릴 수 있음
const TCHAR* const LEVEL_MAP_PATHS[] =
{
    TEXT("/Game/Maps/Level/LV_1"),
    TEXT("/Game/Maps/Level/LV_2"),
    TEXT("/Game/Maps/Level/LV_3"),
    TEXT("/Game/Maps/Level/LV_4")
};

//레벨 개수 배열 길이에서 계산하므로 맵을 추가해도 여기는 안 고쳐도 되고 이 개수를 다 깨면 Endless가 열림
const int32 LEVEL_COUNT = static_cast<int32>(UE_ARRAY_COUNT(LEVEL_MAP_PATHS));

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

//메인 메뉴에서 새 게임 시작
void UDreamVeilGameInstance::StartNewGame()
{
    //이전 판 진행도와 증강이 새 판에 남지 않게 비움
    ClearedLevelCount = 0;
    ClearPlayerAugments();

    //메인 메뉴에는 증강을 가진 플레이어가 없으니 저장하지 않고 바로 이동
    //여기서 저장하면 방금 비운 기록 위에 메뉴 화면 폰의 기록이 덮일 수 있음
    UGameplayStatics::OpenLevel(this, LOBBY_MAP_PATH);
}

//로비에서 게임 시작
bool UDreamVeilGameInstance::OpenNextLevel()
{
    //L4까지 다 깼으면 열 레벨이 없음 이때 로비 UI는 Endless 버튼을 보여줘야 함
    if (IsEndlessUnlocked())
    {
        return false;
    }

    //로비는 상점이라 여기서 증강이 바뀔 수 있으므로 떠나기 전에 저장
    SaveCurrentPlayerAugments();

    //깬 레벨 수가 곧 다음 레벨의 인덱스 하나도 안 깼으면 0번인 L1
    UGameplayStatics::OpenLevel(this, LEVEL_MAP_PATHS[ClearedLevelCount]);

    return true;
}

//지금 레벨을 깼을 때
void UDreamVeilGameInstance::CompleteCurrentLevel()
{
    //이번 레벨에서 얻은 증강을 로비로 들고 가야 하므로 맵을 바꾸기 전에 저장
    //저장을 안 하면 새 맵을 로드할 때 플레이어가 새로 만들어지면서 증강이 전부 사라짐
    SaveCurrentPlayerAugments();

    //실수로 두 번 불려도 레벨 개수를 넘지 않게 막음
    ClearedLevelCount = FMath::Min(ClearedLevelCount + 1, LEVEL_COUNT);

    UGameplayStatics::OpenLevel(this, LOBBY_MAP_PATH);
}

//L4까지 다 깨서 Endless가 열렸는지
bool UDreamVeilGameInstance::IsEndlessUnlocked() const
{
    return ClearedLevelCount >= LEVEL_COUNT;
}

//지금 조종 중인 플레이어의 증강 기록을 저장
void UDreamVeilGameInstance::SaveCurrentPlayerAugments()
{
    APlayerController* PlayerController = GetFirstLocalPlayerController();

    if (!PlayerController || !PlayerController->GetPawn())
    {
        return;
    }

    //증강 컴포넌트가 없는 폰이면 null이 넘어가고 SavePlayerAugments가 바로 돌아가서 기존 저장이 지워지지 않음
    SavePlayerAugments(PlayerController->GetPawn()->FindComponentByClass<UDispatchTableComponent>());
}
