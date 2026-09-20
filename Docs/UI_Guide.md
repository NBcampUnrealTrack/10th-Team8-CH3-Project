# DreamVeil UI 작업 가이드

작성: 2026-09-20 / 대상: 혼자서 수요일까지 UI를 다 만들기

이 문서 하나만 보고 작업할 수 있게 적었습니다.
**C++은 건드리지 않아도 됩니다.** 새 C++이 필요한 항목은 9장에 모아뒀으니 수요일에 같이 합니다.

---

## 0. 지금 상태 (먼저 확인)

| 항목 | 상태 |
|---|---|
| C++ 빌드 | **완료**. `OpenMenuWidget`, `CloseMenuWidget`, `HUDWidgetClass`가 블루프린트에 뜹니다 |
| `UMG` 모듈 | Build.cs에 추가됨 |
| 위젯 에셋 | 아직 없음 (WBP_GameOver를 만들었다면 연결만 하면 됨) |
| 인벤토리/상점/강화 C++ | **전부 완성**. UI는 함수만 부르면 됩니다 |

> 에디터를 켰을 때 `Open Menu Widget` 노드가 안 보이면 빌드가 안 된 겁니다.
> 에디터를 끄고 Visual Studio에서 `DreamVeilEditor | Win64 | Development` 빌드 후 다시 켜세요.

---

## 1. 이미 정한 방향 (다시 확인)

| 질문 | 정한 것 |
|---|---|
| 인벤토리 여는 방식 | **I키로 언제든**. 게임은 안 멈춤 (몬스터 계속 움직임), 마우스 커서 표시 |
| 파츠 장착/해제 | **인벤토리에서 언제든** (인벤토리 = 보기 + 장착) |
| 파츠 목록 모양 | **세로 리스트** |
| 강화 / 구매 / 판매 | **전부 컴퓨터에서** |

이 결정 때문에 생기는 중요한 차이 하나:

- **컴퓨터·침대·게임오버 = "메뉴"** → C++ `OpenMenuWidget`을 씀 → 입력 모드가 **UI Only** (캐릭터 조작 잠김)
- **인벤토리 = "메뉴가 아님"** → `OpenMenuWidget`을 **쓰면 안 됨** → 입력 모드는 **Game And UI** (WASD 이동 가능)

인벤토리를 `OpenMenuWidget`으로 띄우면 캐릭터가 안 움직입니다. 인벤토리만 따로 만듭니다 (6-6 참고).

---

## 2. 만들 위젯 목록

| # | 에셋 이름 | 부모 클래스 | 역할 | 누가 띄우나 | 입력 모드 |
|---|---|---|---|---|---|
| 1 | `WBP_HUD` | UserWidget | 체력·스태미나·꿈의 조각·레벨·현재 무기 | C++ 컨트롤러 BeginPlay | 안 바꿈 |
| 2 | `WBP_MenuBase` | UserWidget | 메뉴 공통 뼈대 (직접 화면에 안 띄움) | — | — |
| 3 | `WBP_GameOver` | **WBP_MenuBase** | 사망 화면 + 계속 버튼 | C++ `ShowGameOver` | UI Only |
| 4 | `BPFL_PartText` | Blueprint Function Library | 파츠 이름 글자 만들기 (위젯 아님) | — | — |
| 5 | `WBP_PartEntry` | UserWidget | 파츠 **한 줄** | 인벤토리/컴퓨터가 만듦 | — |
| 6 | `WBP_Inventory` | UserWidget | I키 상시 인벤토리 (보기 + 장착/해제) | BP_MainPlayerController | **Game And UI** |
| 7 | `WBP_Computer` | **WBP_MenuBase** | 상점 구매 / 판매 / 강화 / 소총 구매 | BP_ComputerActor | UI Only |
| 8 | `WBP_Bed` | **WBP_MenuBase** | 다음 레벨로 갈지 확인 | BP_BedActor | UI Only |
| 9 | `WBP_AugmentChoice` | **WBP_MenuBase** | 레벨업 증강 선택 | BP_MainPlayerController | UI Only |
| 10 | `WBP_MainMenu` | UserWidget | 난이도 + 새 게임 | 메인 메뉴 레벨 | UI Only |

저장 위치: `Content/UI/` 폴더를 새로 만들어서 다 넣으세요.

만드는 순서는 11장 체크리스트를 따르세요. **1 → 3 → 4 → 5 → 6 순서가 중요합니다** (뒤로 갈수록 앞 걸 재사용함).

---

## 3. 공통 규칙 (모든 위젯에 해당)

### 3-1. 플레이어와 컴포넌트 찾기

위젯 그래프에서 매번 쓰는 기본 조합입니다.

```
Get Player Character  →  Cast To MainPlayerCharacter  →  (As Main Player Character)
                                                          ├─ Inventory      (인벤토리 컴포넌트)
                                                          ├─ CombatStats    (체력/공격력/방어력)
                                                          ├─ DispatchTable  (증강)
                                                          ├─ PistolWeapon   (권총 컴포넌트)
                                                          └─ RifleWeapon    (소총 컴포넌트)
```

- `Cast To MainPlayerCharacter`이지 `BP_MainPlayerCharacter`가 아닙니다. C++ 클래스로 캐스트해야 위 변수들이 나옵니다.
- 매번 캐스트하지 말고 **위젯에 변수 `Player` (Main Player Character 오브젝트 레퍼런스)와 `Inventory` (Inventory Component 오브젝트 레퍼런스)를 만들어 Construct에서 한 번만 채우세요.**

### 3-2. Event Construct 타이밍 주의 (중요)

`WBP_HUD`는 C++ `PlayerController::BeginPlay`에서 만들어집니다. 이 시점에 캐릭터가 아직 안 들어와 있을 수 있어서 `Get Player Character`가 **비어 있을 수 있습니다.**

그래서 모든 위젯의 초기화는 이렇게 만드세요.

```
Event Construct  →  InitPlayer  (커스텀 이벤트)

[InitPlayer]
  Get Player Character → Cast To MainPlayerCharacter
    ├ Cast 성공 → Player 변수 저장 → Inventory 변수 저장 → BindEvents → RefreshAll
    └ Cast 실패 → Delay 0.1 → InitPlayer 다시 호출 (재시도)
```

- Cast 실패 쪽을 안 만들면 "가끔 HUD가 비어 있음" 같은 잡기 어려운 버그가 납니다.
- `Delay`는 커스텀 **이벤트**에서만 쓸 수 있습니다 (함수 안에서는 못 씀). `InitPlayer`는 함수가 아니라 **Custom Event**로 만드세요.

### 3-3. 이벤트(Event Dispatcher) 구독하는 법

C++ 쪽 이벤트는 전부 `BlueprintAssignable`이라 블루프린트에서 바로 받을 수 있습니다.

1. `Inventory` 변수 핀을 끌어서 놓고
2. `Bind Event to On Inventory Changed` 를 검색해서 놓고
3. `Event` 핀을 끌어서 `Add Custom Event...` 를 고르고
4. 이름을 `HandleInventoryChanged` 라고 짓습니다

**구독은 Construct에서 한 번만.** 매 프레임이나 Refresh 안에서 하면 중복으로 쌓여서 한 번 바뀔 때 열 번 그려집니다.

### 3-4. 이름 규칙

| 접두사 | 용도 | 예 |
|---|---|---|
| `Text_` | Text Block | `Text_DreamShards` |
| `Bar_` | Progress Bar | `Bar_Health` |
| `Button_` | Button | `Button_Close` |
| `VB_` / `HB_` | Vertical/Horizontal Box | `VB_PistolSockets` |
| `SB_` | Scroll Box | `SB_PartList` |
| `Panel_` | Border/Overlay 묶음 | `Panel_Rifle` |

**그래프에서 이름으로 다뤄야 하는 위젯은 Details에서 `Is Variable` 체크를 꼭 켜세요.** (안 켜면 변수로 안 나옵니다)

---

## 4. `WBP_HUD` — 제일 먼저 만들 것

### 4-1. 화면 배치

```
Canvas Panel
├ Bar_Health        (왼쪽 위, 빨강)      Progress Bar
├ Text_Health       (체력바 위에 겹침)   "80 / 100"
├ Bar_Stamina       (체력바 아래, 노랑)  Progress Bar
├ Text_Level        (왼쪽 위)            "Lv.3"
├ Bar_Experience    (Text_Level 옆)      Progress Bar
├ Text_DreamShards  (오른쪽 위)          "꿈의 조각 120"
└ Text_Weapon       (오른쪽 아래)        "권총" / "소총"
```

### 4-2. 연결 표

| 위젯 | 구독할 이벤트 | 이벤트가 주는 값 | 처음 값 읽기 |
|---|---|---|---|
| `Bar_Health` | `CombatStats` → `On Current Health Changed` | `Old Value`, `New Value` | `CombatStats → Get Health Percentage` |
| | `CombatStats` → `On Max Health Changed` | `Old Value`, `New Value` | |
| `Bar_Stamina` | `Player` → `On Stamina Changed` | `Current Stamina`, `Max Stamina` | `Get Current Stamina` / `Get Max Stamina` |
| `Text_DreamShards` | `Inventory` → `On Dream Shards Changed` | `Dream Shards` | `Inventory → Get Dream Shards` |
| `Bar_Experience` | `Player` → `On Experience Changed` | `Current Experience`, `Required Experience` | `Get Current Experience` / `Get Required Experience` |
| `Text_Level` | `Player` → `On Level Up` | `New Level` | `Get Player Level` |
| `Text_Weapon` | `Player` → `On Weapon Changed` | `New Slot` | `Get Current Weapon Slot` |

체력바 채우기:
- 이벤트가 주는 `New Value`를 쓰지 말고 그냥 **`CombatStats → Get Health Percentage`** 를 불러서 `Set Percent` 하세요. 최대 체력이 바뀌는 경우까지 한 번에 맞습니다.
- 스태미나는 `Current Stamina / Max Stamina` 로 직접 나눠야 합니다 (퍼센트 함수가 없음).

### 4-3. 마지막 연결

`BP_MainPlayerController` → `Class Defaults` → `UI` 카테고리
- `HUD Widget Class` = `WBP_HUD`
- `Game Over Widget Class` = `WBP_GameOver`

이 둘을 안 넣으면 화면이 안 뜹니다. 넣는 곳은 **컨트롤러 블루프린트의 Class Defaults**이고, 레벨에 놓인 액터가 아닙니다.

---

## 5. `WBP_MenuBase` + `WBP_GameOver`

### 5-1. `WBP_MenuBase` (직접 띄우지 않는 부모 위젯)

메뉴 위젯 4개가 똑같이 해야 하는 일 두 가지를 여기 한 번만 적습니다.

- 메뉴가 열리면 인벤토리를 화면에서 내린다
- 메뉴가 열려 있는 동안 I키로 인벤토리가 안 열리게 표시를 켠다

```
[Event Construct]
  Get Owning Player → Cast To BP_MainPlayerController
    → Hide Inventory        (6-6에서 만들 함수, 입력 모드는 안 건드림)
    → Set bMenuOpen = true

[Event Destruct]
  Get Owning Player → Cast To BP_MainPlayerController
    → Set bMenuOpen = false
```

> `Close Inventory`가 아니라 **`Hide Inventory`** 를 부르는 이유: `Close Inventory`는 입력 모드를 Game Only로 되돌립니다. 그런데 메뉴는 바로 뒤에 UI Only로 바꿀 참이라 순서가 꼬일 수 있습니다. 화면에서 내리는 일과 입력 모드를 되돌리는 일을 나눠두면 순서를 신경 쓸 필요가 없습니다.

`WBP_GameOver`, `WBP_Computer`, `WBP_Bed`, `WBP_AugmentChoice`를 만들 때 **부모 클래스를 `WBP_MenuBase`로** 고르세요.
(만들 때: Content Browser → 우클릭 → User Interface → Widget Blueprint → **All Classes** 에서 `WBP_MenuBase` 선택)

### 5-2. `WBP_GameOver`

```
Canvas Panel
└ Border (전체, 검정 60%)
  └ Vertical Box (가운데)
    ├ Text_Title     "꿈에서 깨어났다"
    ├ Text_Info      난이도별 안내
    └ Button_Continue  "계속"
```

`Button_Continue` → `On Clicked`:

```
Get Owning Player → Cast To MainPlayerController → Close Menu Widget
Get Game Instance → Cast To DreamVeilGameInstance → Continue After Death
```

- `Continue After Death`가 난이도를 보고 알아서 처리합니다
  - 쉬움: 이번 판에 얻은 것까지 들고 로비로
  - 보통: 레벨 들어가기 전 상태로 로비로
  - 어려움: 새 게임 (L1부터)
- `Text_Info` 채우기: `Get Game Instance → Cast → Get Difficulty` → `Switch on EGameDifficulty` → 난이도별 문구

---

## 6. `WBP_Inventory` — 가장 중요한 부분

### 6-1. 왜 이게 까다로운가 (먼저 읽기)

인벤토리 C++ 함수들은 **파츠를 "번호"로 가리킵니다.**

```
Equip Part   (Part Index)
Sell Part    (Part Index)
Enhance Part (Part Index)
```

이 번호는 `Get Parts`가 돌려주는 배열의 순서입니다. 문제는 여기입니다.

| 상황 | 결과 |
|---|---|
| 0번 파츠를 팔았다 | 1번이 0번으로, 2번이 1번으로 **전부 당겨짐** |
| 강화하다 파츠가 부서졌다 | 마찬가지로 뒤 번호가 전부 당겨짐 |
| 몬스터가 파츠를 떨궜다 | 맨 뒤에 하나 붙음 |

그러니까 **UI가 번호를 들고 있다가 나중에 쓰면 엉뚱한 파츠를 건드립니다.**
"3번 파츠 팔기" 버튼을 눌렀는데 그 사이 0번이 사라졌으면, 실제로는 4번이던 파츠가 팔립니다.

**해결 방법은 하나입니다. 목록이 바뀌면 줄을 고치지 말고 전부 지우고 다시 만듭니다.**
줄 위젯은 만들어질 때마다 번호를 새로 받으니, 번호가 낡을 틈이 없습니다.

거기에 덧붙는 규칙 두 개:
- "지금 고른 파츠"를 변수로 들고 있다면, 다시 그릴 때 **선택을 해제**한다 (그 번호도 낡았으므로)
- 다시 그릴 때 스크롤이 맨 위로 튀므로, **스크롤 위치를 저장했다가 되돌린다**

> 파츠가 수백 개가 되면 이 방식이 느려집니다. 지금은 많아야 수십 개라 전혀 문제 없습니다.
> 나중에 정말 많아지면 `List View` 위젯으로 바꾸면 됩니다.

### 6-2. 화면 배치

```
+------------------------------------------------------------------+
|  [권총 패널]        [ 보유 파츠 (세로 리스트) ]      [소총 패널]  |
|  권총                                                 소총        |
|  데미지  12.4        +----------------------+        데미지  8.0  |
|  연사 0.35초         | 2등급 총구 +3 [장착중]|       연사 0.10초  |
|  사거리 100m         | 1등급 탄창 +0         |       사거리 120m  |
|                      | 3등급 조준기 +1       |                    |
|  [총구] 2등급 +3     | ...                   |      [총구] 비었음  |
|  [탄창] 비었음       +----------------------+      [탄창] 비었음  |
|  [조준기] 비었음                                    [조준기] ...   |
|                                                     [개머리판]     |
|                                                     [앞손잡이]     |
|                                                                    |
|  꿈의 조각  120                                        [닫기 (I)] |
+------------------------------------------------------------------+
```

### 6-3. 위젯 트리 (이 이름 그대로 만드세요)

```
Canvas Panel
├ Border_Background          (Anchor 전체, 검정 Alpha 0.55)
├ HB_Main                    (Anchor 전체, Padding 60)
│ ├ VB_Pistol                (Size: Fill 1.0)
│ │ ├ Text_PistolTitle          "권총"
│ │ ├ Text_PistolDamage         "데미지 12.4"
│ │ ├ Text_PistolFireRate       "연사 간격 0.35초 (초당 2.9발)"
│ │ ├ Text_PistolRange          "사거리 100m"
│ │ ├ Spacer
│ │ └ VB_PistolSockets          (여기 안에 소켓 줄 3개)
│ │   ├ Text_PistolMuzzle       "[총구] 비어 있음"
│ │   ├ Text_PistolMagazine     "[탄창] 비어 있음"
│ │   └ Text_PistolSight        "[조준기] 비어 있음"
│ ├ SB_PartList              (Scroll Box, Size: Fill 1.4)
│ └ Overlay_Rifle            (Size: Fill 1.0)
│   ├ VB_Rifle                  (권총과 같은 구성 + 소켓 5줄)
│   │ ├ Text_RifleTitle / Text_RifleDamage / Text_RifleFireRate / Text_RifleRange
│   │ └ VB_RifleSockets
│   │   ├ Text_RifleMuzzle / Text_RifleMagazine / Text_RifleSight
│   │   ├ Text_RifleStock      "[개머리판] 비어 있음"
│   │   └ Text_RifleForegrip   "[앞손잡이] 비어 있음"
│   └ Text_RifleLocked          "미보유 — 컴퓨터 상점에서 200"  (Visibility: Collapsed)
├ HB_Shards                  (Anchor 하단 왼쪽)
│ ├ Image (조각 아이콘, 없으면 생략)
│ └ Text_DreamShards            "120"
└ Button_Close               (Anchor 하단 오른쪽)  "닫기 (I)"
```

- `Is Variable` 체크: `SB_PartList`, `Text_*` 전부, `Overlay_Rifle`, `Text_RifleLocked`, `VB_Rifle`
- `SB_PartList` 안에는 아무것도 미리 넣지 않습니다. 실행 중에 채웁니다.

### 6-4. `BPFL_PartText` — 파츠 이름 만드는 함수 라이브러리

파츠 이름은 인벤토리·줄 위젯·컴퓨터 세 군데서 씁니다. 세 번 똑같이 만들지 말고 한 곳에 모읍니다.

Content Browser → 우클릭 → Blueprints → **Blueprint Function Library** → 이름 `BPFL_PartText`

함수 4개를 만듭니다. 전부 **Pure** 체크 (실행 핀 없이 값만 돌려줌).

| 함수 | 입력 | 출력 | 내용 |
|---|---|---|---|
| `SlotToText` | `EWeaponPartSlot Slot` | Text | `Select` 노드로 총구/탄창/조준기/개머리판/앞손잡이 |
| `TierToText` | `EWeaponPartTier Tier` | Text | 1등급/2등급/3등급/4등급/보스 |
| `WeaponToText` | `EWeaponSlot Weapon` | Text | 권총/소총 |
| `MakePartTitle` | `FWeaponPart Part` | Text | 아래 참고 |

`MakePartTitle` 내용:
```
Break WeaponPart  →  Weapon / Slot / Tier / EnhanceLevel / bEquipped

Format Text: "{Tier} {Slot} +{Enhance}{Equipped}"
  Tier     = TierToText(Tier)
  Slot     = SlotToText(Slot)
  Enhance  = EnhanceLevel
  Equipped = bEquipped ? "  [장착중]" : ""   (Select 노드)
```

> `Select` 노드 쓰는 법: 빈 곳에 `Select` 검색 → 놓고 → `Index` 핀에 enum을 연결하면
> 자동으로 항목 수만큼 입력 핀이 생깁니다. 거기에 한국어 Text를 하나씩 적으면 됩니다.

### 6-5. `WBP_PartEntry` — 파츠 한 줄

**설계 원칙: 이 줄 위젯은 "보여주기"와 "눌렸다고 알리기"만 합니다. 무엇을 할지는 부모가 정합니다.**

그래야 인벤토리에서는 "장착/해제", 컴퓨터에서는 "선택"으로 같은 줄 위젯을 그대로 재사용할 수 있습니다.
그리고 줄 위젯이 스스로 `Equip Part`를 부르면, 그 직후 목록이 다시 그려지면서 **자기 자신이 지워진 상태에서 코드가 이어지는** 위험한 모양이 됩니다.

#### 위젯 트리
```
Button_Row                  (줄 전체가 버튼. Style → Normal 색을 반투명하게)
└ Horizontal Box
  ├ Text_Title              "2등급 총구 +3  [장착중]"
  ├ Spacer (Fill)
  └ Text_Effect             "공격력 +7.2"  또는 "연사 +8%"
```

#### 변수
| 변수 | 타입 | 설명 |
|---|---|---|
| `PartIndex` | Integer | 이 줄이 가리키는 파츠 번호 |
| `Part` | WeaponPart (구조체) | 표시용 값 |

#### 이벤트 디스패처
`OnPartClicked` — 입력 파라미터 `PartIndex` (Integer) 하나

#### 그래프
```
[Custom Event] Setup (Part Index: int, In Part: WeaponPart)
  Set PartIndex = Part Index
  Set Part      = In Part
  Text_Title  → Set Text ← BPFL_PartText.MakePartTitle(Part)
  Text_Effect → Set Text ← 아래 참고

[Button_Row → On Clicked]
  Call OnPartClicked (PartIndex)      ← 여기서 끝. 아무것도 더 하지 않음
```

`Text_Effect` 채우기 (파츠가 뭘 올려주는지):
```
Inventory Component 클래스의 static 함수 두 개를 그냥 쓸 수 있습니다 (타겟 연결 불필요)
  Get Part Damage Bonus (Part)    → 0보다 크면 "공격력 +{값}"
  Get Part Fire Rate Bonus (Part) → 0보다 크면 "연사 +{값*100}%"
```
- 총구/조준기는 공격력, 탄창/개머리판/앞손잡이는 연사입니다. 둘 중 0이 아닌 쪽만 보여주면 됩니다.

### 6-6. 인벤토리 여닫기 (BP_MainPlayerController에서)

#### 준비: 입력 액션 만들기
1. 기존 IA들이 있는 폴더에 `IA_Inventory` 생성 (Input Action, Value Type: **Digital (bool)**)
2. 기존 `IMC_...` 매핑 컨텍스트를 열어 `IA_Inventory` 추가 → 키 **I**

#### BP_MainPlayerController에 추가할 변수
| 변수 | 타입 | 설명 |
|---|---|---|
| `InventoryWidget` | User Widget (오브젝트 레퍼런스) | 지금 떠 있는 인벤토리. 없으면 None |
| `bMenuOpen` | Boolean | 컴퓨터/침대/게임오버가 열려 있는지 (WBP_MenuBase가 켜고 끔) |

#### 함수 4개

```
[Function] Hide Inventory          ← 화면에서 내리기만. 입력 모드는 안 건드림
  Is Valid (InventoryWidget)?
    true → InventoryWidget → Remove from Parent
         → Set InventoryWidget = None

[Function] Close Inventory         ← I키로 닫을 때
  Hide Inventory
  Set Input Mode Game Only  (Target: Self)
  Set Show Mouse Cursor = false

[Function] Open Inventory
  bMenuOpen == true?  →  아무것도 안 함 (메뉴가 떠 있으면 인벤토리를 열지 않음)
  Create Widget (Class: WBP_Inventory, Owning Player: Self)
  Set InventoryWidget = 결과
  InventoryWidget → Add to Viewport
  Set Input Mode Game And UI
      In Widget to Focus         : InventoryWidget
      In Mouse Lock Mode         : Do Not Lock
      Hide Cursor During Capture : 체크 해제
  Set Show Mouse Cursor = true

[Custom Event] Toggle Inventory
  Is Valid (InventoryWidget)?
    true  → Close Inventory
    false → Open Inventory
```

#### 입력 연결
```
[EnhancedInputAction IA_Inventory] → Started 핀 → Toggle Inventory
```

#### 이 모드에서 실제로 어떻게 되는지 (미리 알고 있기)
| 동작 | 인벤토리 열었을 때 |
|---|---|
| 몬스터 / 시간 | **계속 흐름** (게임을 멈추지 않음) |
| WASD 이동, 점프, 달리기 | **됨** |
| 마우스 시점 회전 | **안 됨** — 마우스가 커서가 됨 (버튼을 누른 채 움직이면 돌아감) |
| 마우스 왼쪽 클릭 | 인벤토리 위에서는 버튼 클릭, 바깥에서는 사격 |

시점이 안 돌아가는 게 불편하면 두 가지 선택지가 있습니다. 수요일에 얘기해주세요.
- (a) 인벤토리를 여는 동안만 사격을 막고 시점은 살리기 → C++ 한 줄 필요
- (b) 그냥 UI Only로 바꾸고 캐릭터도 같이 멈추기 → BP 한 줄로 가능

### 6-7. `WBP_Inventory` 그래프 — 핵심

#### 변수
| 변수 | 타입 | 설명 |
|---|---|---|
| `Player` | Main Player Character | 3-1 참고 |
| `Inventory` | Inventory Component | 3-1 참고 |
| `PistolSocketTexts` | **Map**: `EWeaponPartSlot` → `Text Block` | 소켓 칸 → 글자 위젯 연결표 |
| `RifleSocketTexts` | **Map**: `EWeaponPartSlot` → `Text Block` | 같음 |

> Map 변수 만드는 법: 변수를 만들고 타입 오른쪽의 작은 아이콘(배열/세트/맵)을 눌러 **Map**을 고르면
> 키 타입과 값 타입을 둘 다 고를 수 있습니다.

#### 초기화
```
[Event Construct] → InitPlayer  (3-2의 재시도 패턴)

[InitPlayer 성공 후]
  1) BuildSocketMaps
  2) BindEvents
  3) RefreshAll

[Function] BuildSocketMaps
  PistolSocketTexts → Add (Muzzle,   Text_PistolMuzzle)
                    → Add (Magazine, Text_PistolMagazine)
                    → Add (Sight,    Text_PistolSight)
  RifleSocketTexts  → Add (Muzzle,   Text_RifleMuzzle)
                    → Add (Magazine, Text_RifleMagazine)
                    → Add (Sight,    Text_RifleSight)
                    → Add (Stock,    Text_RifleStock)
                    → Add (Foregrip, Text_RifleForegrip)

[Custom Event] BindEvents          ← Construct에서 딱 한 번만
  Inventory → Bind Event to On Inventory Changed    → HandleInventoryChanged
  Inventory → Bind Event to On Dream Shards Changed → HandleShardsChanged
  Player    → Bind Event to On Weapon Changed       → HandleWeaponChanged

[Custom Event] HandleInventoryChanged   → RefreshAll
[Custom Event] HandleShardsChanged (Dream Shards) → Text_DreamShards → Set Text
[Custom Event] HandleWeaponChanged (New Slot)     → RefreshWeaponPanels
```

#### RefreshAll — 다시 그리기 (6-1의 해결책)

```
[Custom Event] RefreshAll
  -- 1. 스크롤 위치 기억 -----------------------------
  SB_PartList → Get Scroll Offset → 로컬 변수 SavedOffset

  -- 2. 줄 전부 지우기 -------------------------------
  SB_PartList → Clear Children

  -- 3. 지금 파츠 목록을 새로 받아서 다시 만들기 -----
  Inventory → Get Parts → For Each Loop with Index
     Create Widget (Class: WBP_PartEntry, Owning Player: Get Owning Player)
       → (반환값) Bind Event to OnPartClicked → HandlePartClicked
       → Setup (Part Index: Array Index, In Part: Array Element)
       → SB_PartList → Add Child

  -- 4. 스크롤 되돌리기 ------------------------------
  Completed → SB_PartList → Set Scroll Offset (SavedOffset)

  -- 5. 나머지 갱신 ----------------------------------
  → RefreshWeaponPanels
  → Text_DreamShards → Set Text ← Inventory → Get Dream Shards
```

**여기서 절대 하면 안 되는 것**
- 줄 위젯을 지우지 않고 글자만 바꿔치기 → 번호가 낡아서 엉뚱한 파츠가 장착/판매됨
- `Array Index`를 안 쓰고 자기 나름의 번호를 매기기 → `Get Parts` 순서와 어긋남
- 구독(`Bind Event to On Inventory Changed`)을 RefreshAll 안에 넣기 → 중복 구독으로 몇 배씩 그려짐

#### 줄을 눌렀을 때

```
[Custom Event] HandlePartClicked (Part Index)
  Inventory → Get Parts → Get (a copy) [Part Index] → Break WeaponPart → bEquipped
    true  → Inventory → Unequip Part (Part Index)
    false → Inventory → Equip Part   (Part Index)
  (반환값은 무시해도 됩니다)
```

왜 이걸로 끝나는가:
`Equip Part` / `Unequip Part`가 성공하면 C++이 `OnInventoryChanged`를 쏩니다 →
`HandleInventoryChanged` → `RefreshAll` → 화면이 알아서 최신으로 다시 그려집니다.
**화면을 직접 고치는 코드를 여기 쓰지 마세요.** 한 군데(RefreshAll)에서만 그려야 어긋나지 않습니다.

장착이 실패하는 경우 (알아두기):
- 소총 파츠인데 **소총이 없음** → 실패
- 그 총에 없는 칸 (예: 권총에 개머리판) → 실패
- 실패하면 이벤트가 안 나가므로 화면도 안 바뀝니다. 필요하면 `Equip Part`의 반환값이 false일 때 안내 문구를 띄우세요.

같은 칸에 이미 파츠가 끼워져 있으면 **자동으로 교체**되고 원래 파츠는 인벤토리로 돌아옵니다. 먼저 빼줄 필요 없습니다.

#### RefreshWeaponPanels — 무기 수치 + 소켓

```
[Function] RefreshWeaponPanels

  -- 권총 -------------------------------------------
  Player → PistolWeapon
    → Get Final Damage    → Text_PistolDamage    "데미지 {값}"
    → Get Fire Interval   → Text_PistolFireRate  "연사 간격 {값}초 (초당 {1/값}발)"
    → Get Range           → Text_PistolRange     "사거리 {값/100}m"

  -- 소총 -------------------------------------------
  Player → Has Weapon (Rifle)
    true  → VB_Rifle         Set Visibility = Visible
            Text_RifleLocked Set Visibility = Collapsed
            RifleWeapon 수치를 권총과 똑같이 채움
    false → VB_Rifle         Set Visibility = Hidden   (자리는 차지, 내용은 안 보임)
            Text_RifleLocked Set Visibility = Visible

  -- 소켓 -------------------------------------------
  1) PistolSocketTexts → Values → For Each → Set Text "비어 있음" 로 전부 초기화
     RifleSocketTexts  → Values → For Each → 같게
     (칸 이름까지 같이 쓰려면 Keys로 돌면서 SlotToText를 붙이세요)

  2) Inventory → Get Parts → For Each Loop
       Break WeaponPart
       bEquipped == false → 건너뜀
       Weapon == Pistol → PistolSocketTexts → Find (Slot) → Set Text ← MakePartTitle(Part)
       Weapon == Rifle  → RifleSocketTexts  → Find (Slot) → Set Text ← MakePartTitle(Part)

  -- 현재 들고 있는 무기 강조 ------------------------
  Player → Get Current Weapon Slot
    Pistol → Text_PistolTitle 색 노랑 / Text_RifleTitle 색 회색
    Rifle  → 반대로
```

알아둘 것:
- `Get Final Damage`는 **무기 데미지 + 파츠 + 캐릭터 공격력 + 증강**까지 합친 "실제로 들어가는 데미지"입니다. 그대로 보여주면 됩니다.
- `Get Fire Interval`도 파츠 반영된 값입니다.
- 이 함수들은 **Pure**라서 값이 바뀌어도 알려주지 않습니다. 그래서 `RefreshAll`에서 같이 불러줘야 합니다. (파츠를 끼우면 `OnInventoryChanged` → `RefreshAll` → 여기까지 옵니다)
- `RifleWeapon` 변수는 **소총을 사기 전에도 항상 유효**합니다 (숨겨져 있을 뿐). 보유 여부는 반드시 `Has Weapon`으로 판단하세요.

#### 닫기 버튼
```
[Button_Close → On Clicked]
  Get Owning Player → Cast To BP_MainPlayerController → Close Inventory
```

### 6-8. "먹은 것도 바로 반영" — 자동으로 됩니다

몬스터를 잡으면 C++ `ReceiveKillRewards` → `AddPart` → `OnInventoryChanged` + `OnDreamShardsChanged`가 나갑니다.
인벤토리가 열려 있으면 그대로 `RefreshAll`이 돌아서 즉시 줄이 하나 늘어납니다. 따로 할 게 없습니다.

**획득 알림 문구를 띄우고 싶으면** `OnPartAcquired` (파라미터: `Part`)를 HUD에서 구독해서
`MakePartTitle(Part)` 로 "2등급 총구 획득!" 같은 글자를 2초간 보여주면 됩니다. (선택 사항)

### 6-9. 나중에 여유 있으면 (지금 안 해도 됨)
- 필터 버튼 3개: 전체 / 권총 / 소총 → `Get Parts` 돌 때 조건에 안 맞으면 건너뛰기
  (**주의: 이때도 `Array Index`를 그대로 넘겨야 합니다.** 걸러낸 뒤 새로 번호를 매기면 6-1 문제가 그대로 재현됩니다)
- 정렬: 등급 높은 순 → 마찬가지로 원래 번호를 같이 들고 다녀야 하므로, 정렬은 수요일에 같이 하는 게 안전합니다
- 파츠에 마우스를 올리면 상세 툴팁

---

## 7. `WBP_Computer` — 상점 / 판매 / 강화

부모 클래스: `WBP_MenuBase`

### 7-1. 구성

```
Canvas Panel
└ Border (전체)
  └ Vertical Box
    ├ HB_Tabs
    │ ├ Button_ShopTab      "구매"
    │ ├ Button_SellTab      "판매"
    │ └ Button_EnhanceTab   "강화"
    ├ WidgetSwitcher_Tabs   ← 탭마다 한 장
    │ ├ [0] 구매 화면
    │ ├ [1] 판매 화면
    │ └ [2] 강화 화면
    ├ Text_DreamShards      "꿈의 조각 120"
    └ Button_Close          "닫기"
```

탭 버튼 → `WidgetSwitcher_Tabs` → `Set Active Widget Index (0/1/2)`

### 7-2. 구매 화면

살 수 있는 것: **파츠**(무기×칸×등급 조합) + **소총**

```
파츠 한 줄 = [권총/소총] [칸] [등급] [가격] [구매 버튼]
  구매 버튼 → Inventory → Buy Part (Weapon, Slot, Tier)

등급 버튼 켜고 끄기
  Inventory → Is Tier For Sale (Tier)  → false면 버튼 Disable + "L{n} 클리어 필요"
  가격 표시 : Inventory → Get Buy Price (Tier)

소총 줄
  Inventory → Is Weapon For Sale (Rifle)  → true일 때만 줄을 보여줌
  가격      : Inventory → Get Weapon Price (Rifle)   (200)
  구매 버튼 → Inventory → Buy Weapon (Rifle)
```

- `Is Tier For Sale`은 그 레벨을 깼는지 + 보스 등급이 아닌지를 알아서 판단합니다. UI가 조건을 다시 만들 필요 없습니다.
- `Is Weapon For Sale(Rifle)`은 **L3 해금 + 아직 안 삼** 둘 다 만족할 때만 true입니다.
- 소총을 사면 `OnInventoryChanged`가 나가므로, 인벤토리를 다시 열면 소총 패널이 켜져 있습니다.

### 7-3. 판매 화면 / 강화 화면 — 여기도 `WBP_PartEntry` 재사용

둘 다 "목록에서 하나 고르고 → 오른쪽에서 확인" 구조입니다.

```
Horizontal Box
├ SB_SellList        ← WBP_PartEntry 들을 여기 채움 (6-7의 RefreshAll과 똑같은 방식)
└ VB_Detail
  ├ Text_SelectedPart   "2등급 총구 +3"
  ├ Text_Price          "판매가 18"   또는   "강화 비용 40 / 성공 70% / 파괴 8%"
  └ Button_Do           "판매"        또는   "강화"
```

#### 변수
`SelectedPartIndex` (Integer, 기본값 **-1** = 고른 것 없음)

#### 그래프
```
[줄을 눌렀을 때] HandlePartClicked (Part Index)
  Set SelectedPartIndex = Part Index
  → RefreshDetail

[Function] RefreshDetail
  SelectedPartIndex < 0
    → VB_Detail Set Visibility = Collapsed, 끝
  Inventory → Get Parts → Get (a copy) [SelectedPartIndex] → 로컬 Part
  Text_SelectedPart ← MakePartTitle(Part)

  (판매 탭)  Text_Price ← "판매가 {Get Sell Price(Part)}"
             bEquipped == true → Button_Do Disable + "먼저 빼야 팔 수 있음"
  (강화 탭)  Text_Price ← "비용 {Get Enhance Cost(Part)} / 성공 {Get Enhance Success Chance(Part)*100}%"
                          " / 실패 시 파괴 {Get Enhance Destroy Chance(Part)*100}%"

[Button_Do → On Clicked]  (판매)
  Inventory → Sell Part (SelectedPartIndex)

[Button_Do → On Clicked]  (강화)
  Inventory → Enhance Part (SelectedPartIndex) → 반환값 EPartEnhanceResult
    Switch on EPartEnhanceResult
      Success          → "강화 성공!"
      Fail             → "실패... 조각만 날아갔다"
      Destroyed        → "파츠가 부서졌다"
      NotEnoughShards  → "꿈의 조각이 모자람"
      CannotEnhance    → "더 강화할 수 없음"
    → Text_Result 에 표시

[Custom Event] HandleInventoryChanged     ← 목록이 바뀌면
  Set SelectedPartIndex = -1              ← ★ 번호가 낡았으므로 선택 해제 (6-1 참고)
  → 목록 다시 만들기 (6-7과 동일)
  → RefreshDetail
```

**★ 표시한 줄이 핵심입니다.** 강화로 파츠가 부서지면 그 뒤 번호가 전부 당겨집니다.
선택을 해제하지 않으면, 다음에 "강화" 버튼을 눌렀을 때 **다른 파츠가 강화됩니다.**
강화에 성공했을 때도 똑같이 풀립니다. 결과 문구만 남기고 파츠를 다시 고르게 하세요.

### 7-4. `BP_ComputerActor` 연결

`BP_ComputerActor`의 이벤트 그래프:
```
[Event On Computer Interacted] (Interactor)
  Get Player Controller (0) → Cast To BP_MainPlayerController
    → Open Menu Widget (Menu Widget Class: WBP_Computer)
```

`Open Menu Widget`이 알아서 합니다: 이미 열린 메뉴 닫기 → 새로 띄우기 → UI Only → 커서 켜기.
인벤토리를 내리는 것과 `bMenuOpen`은 `WBP_MenuBase`가 처리합니다 (5-1).

`Button_Close` → `Close Menu Widget`

---

## 8. 나머지 위젯 (짧게)

### 8-1. `WBP_Bed` (부모: WBP_MenuBase)
```
Text_Info   "자고 일어나면 다음 꿈으로 들어간다"
Button_Go   "들어가기"
  → Get Game Instance → Cast To DreamVeilGameInstance → Open Next Level
     (반환값 false = 더 깰 레벨이 없음 → "모든 꿈을 깼다" 문구)
Button_Cancel → Close Menu Widget
```
`BP_BedActor`의 `On Bed Interacted` → `Open Menu Widget (WBP_Bed)`

여유가 되면 `Get Current Level Number`, `Is Endless Unlocked`로 "다음: L3" 같은 안내를 덧붙이세요.

### 8-2. `WBP_AugmentChoice` (부모: WBP_MenuBase)

띄우는 곳은 `BP_MainPlayerController`입니다.
```
[Event BeginPlay] (기존 노드 뒤에 이어서)
  Get Controlled Pawn → Cast To MainPlayerCharacter
    → Bind Event to On Augment Choices Ready → HandleAugmentChoices

[Custom Event] HandleAugmentChoices (Choices: EAugmentID 배열)
  Open Menu Widget (WBP_AugmentChoice) → 반환된 위젯을 Cast To WBP_AugmentChoice
    → Setup (Choices)
```
위젯 안:
```
[Setup] (Choices)
  For Each → 버튼 3개에 하나씩
    Get Augment Display Name (ID)   ← static, 타겟 연결 불필요
    Get Augment Description (ID)

[버튼 클릭]
  Player → Select Augment Choice (ID)
  Player → Has Augment Choices ?
    true  → Setup (Get Current Augment Choices)   ← 한 번에 여러 레벨 올랐을 때 이어서
    false → Close Menu Widget
```

### 8-3. `WBP_MainMenu`
```
Button_Easy / Normal / Hard → Get Game Instance → Cast → Set Difficulty (난이도)
Button_Start                → Start New Game     (진행도·증강·인벤토리 전부 초기화 후 로비로)
Button_Quit                 → Quit Game
```
메인 메뉴 레벨의 레벨 블루프린트 `Event BeginPlay`에서:
`Create Widget (WBP_MainMenu)` → `Add to Viewport` → `Set Input Mode UI Only` → `Set Show Mouse Cursor = true`

---

## 9. 지금 C++에 없어서 못 만드는 것 (수요일에 같이)

| 없는 것 | 왜 필요한가 | 임시 대처 |
|---|---|---|
| **레벨 남은 시간** 조회 | HUD에 제한 시간 카운트다운 | 지금은 표시 생략. `AMainGameModeBase`에 `GetRemainingTime()` 추가 필요 |
| **메뉴가 열려 있는지** 조회 | 인벤토리와 메뉴 충돌 방지 | BP 변수 `bMenuOpen`으로 대신함 (5-1). C++에 `IsMenuOpen()`이 있으면 더 깔끔 |
| `GetWeaponInSlot`이 BP에 없음 | 무기를 슬롯으로 찾기 | `PistolWeapon` / `RifleWeapon` 변수를 직접 읽어서 해결됨 |
| **몬스터 남은 수** | HUD에 "남은 적 12" | `AMainGameModeBase`의 `AliveMonsterCount`가 private |
| **히트마커 / 피격 방향** | 맞췄다는 피드백 | 없음 |
| **세이브** (`USaveGame`) | 게임을 껐다 켜도 유지 | 지금은 껐다 켜면 초기화됨 (설계만 해둠) |
| **보스 증강 풀 분리** | 보스가 플레이어 전용 증강을 먹음 | 기획 확정 후 |

---

## 10. 막혔을 때 보는 표

| 증상 | 원인 | 해결 |
|---|---|---|
| `Open Menu Widget` 노드가 없음 | 빌드 안 됨 | 에디터 끄고 VS에서 빌드 |
| HUD가 화면에 안 뜸 | `HUD Widget Class`가 비어 있음 | BP_MainPlayerController **Class Defaults**에서 지정 |
| HUD 값이 처음에 비어 있음 | Construct 때 캐릭터가 없었음 | 3-2 재시도 패턴 적용 |
| 파츠를 팔았는데 다른 게 팔림 | 번호가 낡음 | 6-1 / 7-3의 `SelectedPartIndex = -1` |
| 목록이 한 번 바뀔 때 여러 번 그려짐 | 구독을 Refresh 안에서 함 | 구독은 Construct에서 한 번만 |
| 인벤토리 열면 캐릭터가 안 움직임 | `Open Menu Widget`으로 띄움 | 6-6의 `Open Inventory` 사용 |
| 메뉴 닫았는데 조작이 안 됨 | 입력 모드가 UI Only로 남음 | `Close Menu Widget`을 부르게 함 (레벨을 옮겨도 BeginPlay가 되돌려 줌) |
| 장착 버튼을 눌러도 아무 일 없음 | 소총 미보유 / 그 총에 없는 칸 | `Equip Part` 반환값을 확인해 안내 문구 띄우기 |
| 다시 그릴 때마다 스크롤이 위로 튐 | 당연함 | `Get/Set Scroll Offset` (6-7) |
| 로비에서 총이 보임 | 무기 칸이 `Nothing`으로 안 바뀜 | 로비 맵 이름이 GameInstance의 `LOBBY_MAP_PATH`와 같은지 확인 |
| 로비에서 총 든 자세로 서 있음 | 애님 BP가 `Nothing`을 처리 안 함 | `Blend Poses (EWeaponSlot)`의 `Nothing Pose` 핀에 맨손 Idle 연결 |

---

## 11. 작업 순서 체크리스트

하루에 하나씩 해도 수요일 전에 끝납니다. 위에서부터 순서대로 하세요.

- [ ] `Content/UI` 폴더 만들기
- [ ] **1일차 — 눈에 보이는 것부터**
  - [ ] `WBP_HUD` 배치 (체력바 / 스태미나바 / 꿈의 조각)
  - [ ] `WBP_HUD` 그래프 연결 (4-2 표)
  - [ ] `BP_MainPlayerController` Class Defaults에 `HUD Widget Class` 지정
  - [ ] PIE: 달리면 스태미나바가 줄고, 1초 뒤 차는지 확인
  - [ ] `WBP_MenuBase` 만들기 (5-1)
  - [ ] `WBP_GameOver`를 `WBP_MenuBase` 자식으로 만들기 + `Game Over Widget Class` 지정
  - [ ] PIE: 콘솔(`~`)에 `CheatDamageMe 9999` → 게임오버 뜨고 계속 버튼이 도는지
- [ ] **2일차 — 인벤토리 준비물**
  - [ ] `BPFL_PartText` 함수 4개 (6-4)
  - [ ] `WBP_PartEntry` (6-5)
  - [ ] `IA_Inventory` 만들고 IMC에 I키 추가
  - [ ] `BP_MainPlayerController`에 변수 2개 + 함수 4개 (6-6)
  - [ ] PIE: I키로 빈 인벤토리가 떴다 사라지는지, WASD가 되는지
- [ ] **3일차 — 인벤토리 본체**
  - [ ] `WBP_Inventory` 배치 (6-3)
  - [ ] `BuildSocketMaps` / `BindEvents` / `RefreshAll` (6-7)
  - [ ] `HandlePartClicked` (장착/해제 토글)
  - [ ] `RefreshWeaponPanels` (수치 + 소켓)
  - [ ] PIE 테스트 (12장)
- [ ] **4일차 — 컴퓨터**
  - [ ] `WBP_Computer` 탭 3개 틀
  - [ ] 구매 탭 (7-2)
  - [ ] 판매 / 강화 탭 (7-3) — `SelectedPartIndex = -1` 잊지 말 것
  - [ ] `BP_ComputerActor` 연결 (7-4)
- [ ] **여유 있으면**
  - [ ] `WBP_Bed`
  - [ ] `WBP_AugmentChoice`
  - [ ] `WBP_MainMenu`

---

## 12. PIE 테스트 시나리오

콘솔은 `~` 키로 엽니다. 치트 함수는 전부 캐릭터에 있습니다.

| # | 할 것 | 기대 결과 |
|---|---|---|
| 1 | 로비에서 I키 | 인벤토리가 뜨고 커서가 보임. WASD로 움직여짐 |
| 2 | I키 다시 | 닫히고 커서 사라짐. 조작 정상 |
| 3 | 레벨에 들어가 몬스터 잡기 | 꿈의 조각이 HUD와 인벤토리에서 **동시에** 오름 |
| 4 | 인벤토리를 **열어둔 채** 몬스터 잡기 | 파츠를 떨구면 줄이 즉시 하나 늘어남 |
| 5 | 파츠 줄 클릭 | `[장착중]`이 붙고, 왼쪽 소켓 칸에 이름이 들어감 |
| 6 | 같은 칸의 다른 파츠 클릭 | 새 것이 끼워지고 원래 것의 `[장착중]`이 사라짐 |
| 7 | 장착 후 권총 데미지 확인 | `데미지` 숫자가 파츠 효과만큼 올라감 |
| 8 | `CheatAcquireRifle` 후 인벤토리 | 오른쪽 소총 패널이 켜지고 소켓 5칸이 보임 |
| 9 | 소총 파츠를 소총 사기 **전에** 클릭 | 아무 일도 안 일어남 (정상. 안내 문구는 선택) |
| 10 | 컴퓨터에서 파츠 3개 사고 가운데 것 판매 | 목록이 당겨지고, 선택이 풀리며, 남은 2개가 맞게 표시됨 |
| 11 | 강화를 파괴될 때까지 반복 | 파괴돼도 목록이 어긋나지 않음. 선택이 풀림 |
| 12 | 인벤토리를 연 채 컴퓨터 E | 인벤토리가 사라지고 컴퓨터 창만 뜸. 닫으면 조작 정상 |
| 13 | 컴퓨터가 열린 상태에서 I키 | 인벤토리가 **안 열림** (`bMenuOpen` 때문) |
| 14 | `CheatDamageMe 9999` | 게임오버 → 계속 → 난이도에 맞게 로비/L1로 |

값이 화면에 안 보여서 답답하면 `Print String`을 잠깐 물려두세요. `Duration`을 5초쯤 주면 읽기 편합니다.

---

## 13. 블루프린트에서 쓸 수 있는 C++ 목록 (찾아보기용)

### `AMainPlayerCharacter` — `Get Player Character` → `Cast To MainPlayerCharacter`
| 이름 | 종류 | 설명 |
|---|---|---|
| `Inventory` | 변수 | 인벤토리 컴포넌트 |
| `CombatStats` | 변수 | 체력·공격력·방어력 |
| `DispatchTable` | 변수 | 증강 |
| `PistolWeapon` / `RifleWeapon` | 변수 | 무기 컴포넌트 (수치 읽기용) |
| `Get Current Weapon` / `Get Current Weapon Slot` | Pure | 지금 든 무기. 로비에서는 맨손이라 `Nothing` / `None` |
| `Has Weapon (Slot)` | Pure | 소총 보유 여부 |
| `Equip Weapon (Slot)` | Call | 무기 바꿔 들기 |
| `Get Current Stamina` / `Get Max Stamina` | Pure | |
| `Get Player Level` / `Get Current Experience` / `Get Required Experience` | Pure | |
| `Get Current Augment Choices` / `Has Augment Choices` / `Get Pending Augment Choice Count` | Pure | |
| `Select Augment Choice (ID)` | Call | 증강 고르기 |
| `On Weapon Changed` (New Slot) | 이벤트 | |
| `On Player Died` | 이벤트 | C++ 컨트롤러가 이미 씀 |
| `On Stamina Changed` (Current, Max) | 이벤트 | |
| `On Experience Changed` (Current, Required) | 이벤트 | |
| `On Level Up` (New Level) | 이벤트 | |
| `On Augment Choices Ready` (Choices) | 이벤트 | |

### `UCombatStatsComponent`
`Get Current Health` / `Get Max Health` / `Get Health Percentage` / `Get Attack Power` / `Get Defence Power` / `Is Dead`
이벤트: `On Current Health Changed` (Old, New), `On Max Health Changed` (Old, New), `On Dead`

### `UInventoryComponent`
| 이름 | 종류 | 설명 |
|---|---|---|
| `Get Parts` | Pure | 파츠 전부. **이 순서가 곧 Part Index** |
| `Get Dream Shards` | Pure | |
| `Equip Part (Index)` / `Unequip Part (Index)` | Call | |
| `Enhance Part (Index)` | Call | 반환 `EPartEnhanceResult` |
| `Buy Part (Weapon, Slot, Tier)` | Call | |
| `Sell Part (Index)` | Call | 낀 파츠는 실패 |
| `Buy Weapon (Weapon)` | Call | 소총 |
| `Is Tier For Sale (Tier)` / `Is Weapon For Sale (Weapon)` | Pure | 버튼 켜고 끄기 |
| `Get Buy Price (Tier)` / `Get Sell Price (Part)` / `Get Weapon Price (Weapon)` | Pure (static) | |
| `Get Enhance Cost / Success Chance / Destroy Chance (Part)` | Pure (static) | |
| `Get Part Damage Bonus (Part)` / `Get Part Fire Rate Bonus (Part)` | Pure (static) | 줄 표시용 |
| `Add Part` / `Add Dream Shards` | Call | 테스트용. 평소엔 C++이 부름 |
| `On Inventory Changed` | 이벤트 | **목록이 바뀜 → 다시 그리기** |
| `On Dream Shards Changed` (Dream Shards) | 이벤트 | |
| `On Part Acquired` (Part) | 이벤트 | 획득 알림용 |

### `UWeaponBase` (`PistolWeapon` / `RifleWeapon`)
`Get Final Damage` (파츠·증강·공격력 다 포함) / `Get Fire Interval` (파츠 포함) / `Get Range` / `Is Automatic` / `Get Part Slots`

### `UDispatchTableComponent`
`Get Augment Display Name (ID)` / `Get Augment Description (ID)` (둘 다 static) / `Has Acquired Augment (ID)` / `Get Augment History`

### `UDreamVeilGameInstance` — `Get Game Instance` → `Cast To DreamVeilGameInstance`
`Start New Game` / `Open Next Level` / `Complete Current Level` / `Fail Current Level` / `Continue After Death`
`Is Endless Unlocked` / `Is In Level Map` / `Is In Lobby` / `Get Current Level Number` / `Is Level Unlocked (n)`
`Set Difficulty` / `Get Difficulty`

### `AMainPlayerController`
`Open Menu Widget (Class)` → 만든 위젯을 돌려줍니다 / `Close Menu Widget`
Class Defaults: `Game Over Widget Class`, `HUD Widget Class`

### 구조체 `FWeaponPart` — `Break WeaponPart`
`Weapon` (권총/소총) · `Slot` (총구/탄창/조준기/개머리판/앞손잡이) · `Tier` (1~4/보스) · `EnhanceLevel` (0~5) · `bEquipped`

### enum `EWeaponSlot`
`Pistol` · `Rifle` · `Nothing`(맨손, 로비 전용 상태)
`Nothing`은 무기가 아니라 **상태**입니다. 파츠에는 절대 쓰지 마세요.
UI에서 무기 이름을 뽑을 때(`WeaponToText`) `Nothing`이 들어오면 빈 글자를 돌려주면 됩니다.
