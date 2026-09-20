#include "DreamVeilGameInstance.h"

#include "DispatchTableComponent.h"
#include "InventoryComponent.h"
#include "MainPlayerCharacter.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"

//로비 맵 경로 메인 메뉴에서 들어오고 레벨을 깰 때마다 돌아오는 곳
const TCHAR* const LOBBY_MAP_PATH = TEXT("/Game/Maps/Level/Lobby");

//메인 메뉴 맵 경로 게임을 켜면 처음 뜨고 어려움에서 죽었을 때 돌아오는 곳
//레벨 맵이 아니므로 Maps/Level 폴더 밖에 둠 레벨 번호를 세는 LEVEL_MAP_PATHS와 섞이지 않게 하려는 것
const TCHAR* const MAIN_MENU_MAP_PATH = TEXT("/Game/Maps/MainMenu");

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

//난이도가 몬스터 스폰 곡선 경사(AMonsterSpawnVolume::DifficultyCurve)에 거는 배율 순서는 쉬움 보통 어려움
//그 값은 작을수록 초반부터 가파르게 어려워지므로 쉬움은 1보다 크게 어려움은 1보다 작게 둠
//여기 숫자만 바꾸면 난이도별 체감이 조절됨
const float SPAWN_CURVE_SCALE_BY_DIFFICULTY[] = { 1.6f, 1.0f, 0.5f };

//난이도 수와 표의 칸 수가 어긋나면 컴파일 단계에서 바로 알 수 있게 막음
static_assert(static_cast<int32>(UE_ARRAY_COUNT(SPAWN_CURVE_SCALE_BY_DIFFICULTY)) == static_cast<int32>(EGameDifficulty::Hard) + 1, "SPAWN_CURVE_SCALE_BY_DIFFICULTY needs one value per difficulty");

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
    ClearRunProgress();

    //메인 메뉴에는 증강을 가진 플레이어가 없으니 저장하지 않고 바로 이동
    //여기서 저장하면 방금 비운 기록 위에 메뉴 화면 폰의 기록이 덮일 수 있음
    UGameplayStatics::OpenLevel(this, LOBBY_MAP_PATH);
}

//메인 메뉴로 나감
void UDreamVeilGameInstance::OpenMainMenu()
{
    //메뉴로 나가면 하던 판은 끝난 것이라 진행도와 얻은 것을 비움
    //안 비우면 메뉴에서 새 게임을 누르기 전까지 이전 판 기록이 남아 있게 됨
    ClearRunProgress();

    UGameplayStatics::OpenLevel(this, MAIN_MENU_MAP_PATH);
}

//한 판의 진행도와 얻은 것을 전부 비움
//새 게임과 어려움 사망이 똑같이 해야 하는 일이라 한곳에 모음 한쪽만 고쳐서 어긋나는 걸 막으려는 것
void UDreamVeilGameInstance::ClearRunProgress()
{
    ClearedLevelCount = 0;
    ClearPlayerAugments();
    ClearPlayerInventory();
}

//로비에서 게임 시작
bool UDreamVeilGameInstance::OpenNextLevel()
{
    //L4까지 다 깼으면 열 레벨이 없음 이때 로비 UI는 Endless 버튼을 보여줘야 함
    if (IsEndlessUnlocked())
    {
        return false;
    }

    //로비는 상점이라 여기서 파츠와 꿈의 조각이 바뀌므로 떠나기 전에 증강과 같이 저장
    //이때 저장한 것이 레벨에 들어가기 전 상태가 되어 보통 난이도에서 실패하면 여기로 돌아옴
    SaveCurrentPlayerProgress();

    //깬 레벨 수가 곧 다음 레벨의 인덱스 하나도 안 깼으면 0번인 L1
    UGameplayStatics::OpenLevel(this, LEVEL_MAP_PATHS[ClearedLevelCount]);

    return true;
}

//지금 레벨을 깼을 때
void UDreamVeilGameInstance::CompleteCurrentLevel()
{
    //이번 레벨에서 얻은 증강과 파츠를 로비로 들고 가야 하므로 맵을 바꾸기 전에 저장
    //저장을 안 하면 새 맵을 로드할 때 플레이어가 새로 만들어지면서 증강이 전부 사라짐
    SaveCurrentPlayerProgress();

    //실수로 두 번 불려도 레벨 개수를 넘지 않게 막음
    ClearedLevelCount = FMath::Min(ClearedLevelCount + 1, LEVEL_COUNT);

    UGameplayStatics::OpenLevel(this, LOBBY_MAP_PATH);
}

//제한 시간 안에 못 깼거나 쉬움 보통에서 죽었을 때
//죽는 것도 실패의 한 종류라 규칙을 따로 두지 않고 여기 하나로 처리함
void UDreamVeilGameInstance::FailCurrentLevel()
{
    //보통과 어려움은 저장하지 않고 떠나서 로비의 플레이어는 이 레벨에 들어오기 전 상태로 복원됨
    //실패해도 얻은 걸 남기면 쉬운 레벨을 일부러 시간 초과시키면서 증강과 파츠만 모을 수 있어서 버림
    //진행도(ClearedLevelCount)도 그대로라 로비에서 게임 시작을 누르면 같은 레벨을 다시 도전함

    //쉬움은 이번 판에 얻은 증강 인벤토리 무기까지 전부 저장 쉬움은 모으기 편하게 하려는 난이도라 위 꼼수를 막지 않음
    if (Difficulty == EGameDifficulty::Easy)
    {
        SaveCurrentPlayerProgress();
    }

    UGameplayStatics::OpenLevel(this, LOBBY_MAP_PATH);
}

//플레이어가 죽은 뒤 이어서 진행
void UDreamVeilGameInstance::ContinueAfterDeath()
{
    //어려움은 죽으면 끝 진행도 증강 인벤토리 무기를 전부 잃고 메인 메뉴로 쫓겨남
    //로비가 아니라 메뉴로 보내는 이유 로비로 보내면 바로 다시 들어갈 수 있어서 죽은 대가가 약해짐
    //다시 하려면 메뉴에서 난이도를 고르고 게임 시작을 눌러야 함
    if (Difficulty == EGameDifficulty::Hard)
    {
        OpenMainMenu();
        return;
    }

    //쉬움 보통은 시간 초과와 똑같이 로비로 감 로비에서 파츠를 사고 강화하거나 바로 다시 도전할지 고름
    //쉬움은 여기서 전부 저장되고 보통은 레벨에 들어오기 전 상태로 돌아감
    FailCurrentLevel();
}

//L4까지 다 깨서 Endless가 열렸는지
bool UDreamVeilGameInstance::IsEndlessUnlocked() const
{
    return ClearedLevelCount >= LEVEL_COUNT;
}

//지금 조종 중인 플레이어의 증강 기록과 인벤토리를 저장
void UDreamVeilGameInstance::SaveCurrentPlayerProgress()
{
    APawn* PlayerPawn = FindCurrentPlayerPawn();

    if (!PlayerPawn)
    {
        return;
    }

    //증강 컴포넌트가 없는 폰이면 null이 넘어가고 SavePlayerAugments가 바로 돌아가서 기존 저장이 지워지지 않음
    SavePlayerAugments(PlayerPawn->FindComponentByClass<UDispatchTableComponent>());

    //인벤토리도 같은 규칙 컴포넌트가 없는 폰이면 기존 저장을 그대로 둠
    SavePlayerInventory(PlayerPawn->FindComponentByClass<UInventoryComponent>());
}

//지금 조종 중인 플레이어 폰
APawn* UDreamVeilGameInstance::FindCurrentPlayerPawn() const
{
    APlayerController* PlayerController = GetFirstLocalPlayerController();

    return PlayerController ? PlayerController->GetPawn() : nullptr;
}

//지금 맵이 L1~L4 중 하나인지
//레벨 목록을 이 파일이 들고 있으니 다른 곳에서 맵 이름을 따로 적지 않고 여기서 판단함
bool UDreamVeilGameInstance::IsInLevelMap() const
{
    //레벨 번호가 0이면 로비나 메인 메뉴
    return GetCurrentLevelNumber() > 0;
}

//지금 맵의 레벨 번호
int32 UDreamVeilGameInstance::GetCurrentLevelNumber() const
{
    //PIE에서는 맵 이름 앞에 UEDPIE_0_ 같은 접두사가 붙는데 두 번째 인자 true가 그걸 떼줌
    const FString CurrentMapName = UGameplayStatics::GetCurrentLevelName(this, true);

    for (int32 LevelIndex = 0; LevelIndex < LEVEL_COUNT; ++LevelIndex)
    {
        //전체 경로에서 끝의 맵 이름만 떼서 비교 /Game/Maps/Level/LV_1 -> LV_1
        if (FPackageName::GetShortName(LEVEL_MAP_PATHS[LevelIndex]) == CurrentMapName)
        {
            //배열은 0번부터라 1을 더해서 L1이 1이 되게
            return LevelIndex + 1;
        }
    }

    return 0;
}

//지금 맵이 로비인지
bool UDreamVeilGameInstance::IsInLobby() const
{
    //레벨 번호를 찾을 때와 같은 방식 PIE 접두사를 뗀 맵 이름과 로비 맵 이름을 비교함
    return UGameplayStatics::GetCurrentLevelName(this, true) == FPackageName::GetShortName(LOBBY_MAP_PATH);
}

//그 레벨에 들어갈 수 있는지
bool UDreamVeilGameInstance::IsLevelUnlocked(int32 LevelNumber) const
{
    //깬 레벨 수 + 1번 레벨까지 열려 있음 하나도 안 깼으면 L1만
    return LevelNumber >= 1 && LevelNumber <= LEVEL_COUNT && LevelNumber <= ClearedLevelCount + 1;
}

//난이도를 정함
void UDreamVeilGameInstance::SetDifficulty(EGameDifficulty NewDifficulty)
{
    Difficulty = NewDifficulty;
}

//지금 난이도
EGameDifficulty UDreamVeilGameInstance::GetDifficulty() const
{
    return Difficulty;
}

//난이도가 몬스터 스폰 곡선 경사에 거는 배율
//enum 값을 그대로 표의 번호로 쓰므로 난이도를 추가하면 위 표에도 값을 하나 더해야 함(static_assert가 잡아줌)
float UDreamVeilGameInstance::GetSpawnCurveScale() const
{
    return SPAWN_CURVE_SCALE_BY_DIFFICULTY[static_cast<int32>(Difficulty)];
}

//플레이어 인벤토리를 저장
void UDreamVeilGameInstance::SavePlayerInventory(UInventoryComponent* PlayerInventory)
{
    //인벤토리가 없는 폰이면 기존 저장을 지우지 않고 그대로 둠
    if (!PlayerInventory)
    {
        return;
    }

    SavedParts = PlayerInventory->GetParts();
    SavedDreamShards = PlayerInventory->GetDreamShards();

    //상점에서 산 소총도 새 레벨에서는 캐릭터가 새로 만들어져 사라지므로 파츠와 같이 저장
    //난이도 규칙도 파츠와 똑같이 받음 보통에서 죽으면 그 레벨에서 산 무기는 없음
    if (const AMainPlayerCharacter* OwnerPlayer = Cast<AMainPlayerCharacter>(PlayerInventory->GetOwner()))
    {
        SavedWeaponSlots = OwnerPlayer->GetAcquiredWeaponSlots();
    }
}

//저장한 인벤토리를 새 레벨의 플레이어에게 복원
void UDreamVeilGameInstance::RestorePlayerInventory(UInventoryComponent* PlayerInventory)
{
    if (!PlayerInventory)
    {
        return;
    }

    //무기를 먼저 돌려줌 인벤토리는 가진 총에만 파츠를 끼우므로 순서가 바뀌면 소총 파츠가 소총에 반영되지 않음
    if (AMainPlayerCharacter* OwnerPlayer = Cast<AMainPlayerCharacter>(PlayerInventory->GetOwner()))
    {
        for (EWeaponSlot WeaponSlot : SavedWeaponSlots)
        {
            OwnerPlayer->AcquireWeapon(WeaponSlot);
        }
    }

    //첫 레벨이면 저장본이 비어 있어서 빈 인벤토리로 시작함
    PlayerInventory->RestoreInventory(SavedParts, SavedDreamShards);
}

//저장한 인벤토리를 비움
void UDreamVeilGameInstance::ClearPlayerInventory()
{
    SavedParts.Empty();
    SavedDreamShards = 0;
    SavedWeaponSlots.Empty();
}
