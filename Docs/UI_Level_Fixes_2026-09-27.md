# UI·잠식도·카메라 수정 (2026-09-27UI)

## 적용 범위

- 기존 `WBP_Bed` 디자인을 확인창으로 재사용하고 `WBP_DreamSelect`를 추가했습니다.
- `WBP_AugmentChoice`에 제공받은 증강 아이콘 12개를 연결했습니다.
- 제한 시간 종료(잠식도 100%) 시 기존 게임 오버 화면을 열도록 연결했습니다.
- 캐릭터 카메라의 어깨 위치 보정을 SpringArm의 충돌 검사에 포함했습니다.
- 캐릭터 재질, 레벨 조명, 막힌 통로 표현, 빨간 메모리 경고는 수정하지 않았습니다.
- 플러그인 설치·활성화 없이 프로젝트 소스와 에셋을 수정했습니다. `DreamVeilEditorTools`는 에디터 전용 프로젝트 모듈이며 게임 실행 모듈이 아닙니다.

## 침대 UI

`BP_BedActor`의 기본값:

- Confirm Widget Class: `WBP_Bed`
- Dream Select Widget Class: `WBP_DreamSelect`

흐름은 침대 상호작용 → 확인창 → 예 → 꿈 선택창입니다. 취소/닫기는 기존 `CloseBedMenu`를 호출합니다. 확인창의 제목은 “꿈에 들어가겠습니까?”이고 선택창은 “어떤 꿈에 들어가겠습니까?”입니다.

블루프린트 이벤트는 `DreamUIBlueprintLibrary`의 호출 노드에 연결되어 있습니다.

- 확인창 예: `Open Bed Dream Select` → BedActor의 `OpenDreamSelect`
- 취소/닫기: `Close Bed Dream Menu` → BedActor의 `CloseBedMenu`
- 선택창 Construct: 부모 Construct → `Refresh Dream Selection`
- L1~L4: `Enter Dream`(1~4) → GameInstance의 `OpenLevelByNumber`
- Endless: `Enter Dream`(0) → GameInstance의 `OpenEndless`

해금/클리어 여부는 팀원 C++의 `IsLevelUnlocked`, `IsLevelCleared`, `IsEndlessUnlocked`를 사용합니다. 잠긴 버튼은 비활성화하고 “잠김”, 클리어한 레벨은 “클리어”를 표시합니다. 이미 클리어한 레벨의 재입장 규칙은 변경하지 않았습니다. 로비 메뉴는 게임을 일시 정지하지 않습니다.

## 증강 UI

카드 안에서 아이콘 → 이름 → 설명 순서입니다. 원래의 선택지 생성/선택 처리와 설명은 유지했습니다.

`Setup(Choices)` → `Apply Augment Icons` → 기존 Choice1/2/3 저장 및 이름/설명 설정 노드로 이어집니다. 아이콘은 파일의 순번이 아니라 실제 `EAugmentID`로 찾아 연결합니다.

| 제공 이미지 | 증강 ID |
|---|---|
| 01_bullet | AttackUp |
| 02_shield_up | DefenceUp |
| 03_healing | HealthUp |
| 04_speed_boot | StaminaUp |
| 05_berserk | Berserker |
| 06_fortress | LastFortress |
| 07_thorns | ThornArmor |
| 08_vampire | Vampire |
| 09_regeneration | Regeneration |
| 10_sticky_ground | SlowEnemy |
| 11_target_three_monsters | AreaAttack |
| 12_fireball | ContinuousAttack |

텍스처와 연결 데이터는 `Content/UI/AugmentIcons`에 있습니다. `DA_AugmentIcons`가 텍스처를 직접 참조하므로 패키징 시에도 참조가 유지됩니다.

## 잠식도 구조 — 팀 공유용

잠식도는 별도로 누적하는 수치가 아니라 **레벨 제한 시간의 경과 비율**입니다.

1. `MainGameModeBase`가 레벨 제한 시간 타이머를 관리합니다.
2. `GetLevelTimeProgress`에서 `Clamp(1 - 남은 시간 / 제한 시간, 0, 1)`을 반환합니다.
3. `WBP_Corruption`의 Construct에서 GameMode를 참조하고, Tick → UpdateCorruption에서 이 비율을 가져옵니다.
4. 비율을 `Image_Corruption`의 `Set Render Opacity`에 전달해 잠식 연출을 진하게 합니다.
5. 남은 시간은 분/초로 나누어 타이머 텍스트에 표시합니다.
6. 제한 시간 종료 → `FailLevel` → 스폰/레벨 타이머 정지 → `ShowCorruptionGameOver` → 기존 `WBP_GameOver` 표시 및 게임 일시 정지입니다.
7. 게임 오버의 확인 버튼 → `Continue Dream Game Over`: 잠식도 초과이면 `FailCurrentLevel`, 일반 사망이면 `ContinueAfterDeath`를 호출합니다.

따라서 **Hard 난이도의 시간 초과를 사망으로 처리하여 세이브를 삭제하지 않습니다.** 기존 규칙대로 Easy는 이번 레벨 보상을 유지하고, Normal/Hard 시간 초과는 입장 전 상태로 로비에 돌아갑니다. 이미 게임 오버가 열린 뒤 다른 메뉴가 덮어쓰지 않도록 막았습니다.

## 카메라

기존 카메라 컴포넌트의 Y 위치 40은 SpringArm 충돌 검사 후에 더해져 벽 안쪽으로 들어갈 수 있었습니다. 이 위치를 `SocketOffset`으로 옮기고 카메라 자체 위치를 0으로 맞춰 검사 대상에 포함했습니다. Camera 채널 검사와 최소 Probe Size 20을 적용했습니다.

`BP_MainPlayerCharacter`의 저장된 카메라 기본값과 C++ BeginPlay에 적용합니다. 원본 파일에 남아 있던 Git LFS 충돌 표시는 사용자 선택에 따라 UI 쪽 버전으로 복구한 뒤 수정했습니다.

## 검증 및 플레이 확인

- 변경된 블루프린트 6개 컴파일, 아이콘 12개 참조, 이미지 슬롯 3개, 선택창 클릭 경로 6개, 침대 위젯 클래스 연결을 검사했습니다.
- Lobby/LV1~4의 Wall 메시가 Camera 채널을 Block하는 것을 확인했습니다. 맵 자체는 저장/변경하지 않았습니다.
- 실제 플레이에서 침대 예/취소/닫기, L1~Endless 해금 단계, 잠식도 100% 후 로비 복귀, 벽/모서리에서 카메라 회전을 최종 확인해야 합니다.
- 기존 `EliteMonsterAIController`의 `/Game/Managers/AI/BT_EliteMonster` 누락 오류는 이번 수정 범위 밖이며 그대로 남아 있습니다.

검증 로그는 `Saved/UIUpdate*.log`, UI 그래프 감사 결과는 `Saved/DreamVeilUIAudit.txt`에 생성됩니다. 검증 도구의 `-Verify`는 에셋을 저장하지 않습니다. `-Apply`는 이미 적용된 선택창이 있으면 중복 적용을 거부합니다. `-Finalize`만 기존 적용 에셋의 보정값을 저장합니다.

수정 전 백업: `C:/Users/2mir0/Documents/Codex/2026-09-22/new-chat/work/revision-20260927/`.
커밋/푸시는 수행하지 않았습니다.
