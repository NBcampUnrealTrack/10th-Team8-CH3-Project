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
| 9 | `WBP_AugmentCard` | UserWidget | 증강 **카드 한 장** (그림+이름+설명) | 8-2가 미리 배치 | — |
| 10 | `WBP_AugmentChoice` | **WBP_MenuBase** | 증강 카드 3장 중 하나 고르기 | BP_MainPlayerController | UI Only |
| 11 | `WBP_MainMenu` | UserWidget | 난이도 + 새 게임 | 메인 메뉴 레벨 | UI Only |

저장 위치: `Content/UI/` 폴더를 새로 만들어서 다 넣으세요.

**팀 공통 규칙: 뷰포트에 직접 올리는 위젯은 전부 `Scale Box(Scale To Fit)` + `Size Box(1920 x 1080, Slot 정렬 Center)`로 감싸고, Designer 미리보기는 `1920 x 1080` / `Desired on Screen`에서 봅니다.** 이유와 설정법은 3-6.

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

#### Function과 Custom Event 중 뭘로 만드나

| | **Function** (`My Blueprint → Functions → +`) | **Custom Event** (그래프에 `Add Custom Event`) |
|---|---|---|
| `Delay` 같은 지연 노드 | **못 씀** | 씀 |
| 값 돌려주기 | 됨 | 안 됨 |
| 다른 **함수 안에서** 호출 | 됨 | **안 됨** |

| **로컬 변수** 만들기 | **됨** | **안 됨** (멤버 변수를 써야 함) |

- 기다릴 게 없고 / 다른 함수 안에서 불러야 하고 / 로컬 변수가 필요하면 → **Function**
  (`SetButtonHighlight`, `RefreshDifficulty`, `RefreshAll`, `RefreshWeaponPanels`, `RefreshDetail`)
- `Delay`로 재시도하거나 **이벤트 구독의 받는 쪽**이면 → **Custom Event**
  (`InitPlayer`, `HandleInventoryChanged`, `HandlePartClicked`, `BindEvents`)

> 이 문서에서 `[Function]` / `[Custom Event]` 라고 앞에 붙여둔 그대로 만드시면 됩니다.
>
> 흔한 구조는 **Custom Event가 받아서 Function을 부르는** 것입니다.
> `Bind Event to On Inventory Changed` → `HandleInventoryChanged`(Custom Event) → `RefreshAll`(Function)

#### 로컬 변수 만드는 법

**함수 그래프를 연 상태**에서만 왼쪽 `My Blueprint` 패널에 **`Local Variables`** 섹션이 나타납니다.
`+` → 이름 입력 → 타입 고르기 → 그래프로 드래그하면 `Get` / `Set` 중에 고를 수 있습니다.

Event Graph나 Custom Event에는 이 섹션이 아예 없습니다. 거기서 값을 잠깐 담아둬야 하면 일반(멤버) 변수를 쓰거나, 그 부분을 Function으로 빼세요.

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
| `Text_` | Text | `Text_DreamShards` |
| `Bar_` | Progress Bar | `Bar_Health` |
| `Button_` | Button | `Button_Close` |
| `VB_` / `HB_` | Vertical/Horizontal Box | `VB_PistolSockets` |
| `SB_` | Scroll Box | `SB_PartList` |
| `Panel_` | Border/Overlay 묶음 | `Panel_Rifle` |

**그래프에서 이름으로 다뤄야 하는 위젯은 Details에서 `Is Variable` 체크를 꼭 켜세요.** (안 켜면 변수로 안 나옵니다)

### 3-4-1. Palette에서 뭘 끌어와야 하는지 (전체 표)

이 문서에 나오는 이름과 Palette에 보이는 이름이 다른 게 있어서 정리합니다.

| 문서의 이름 | **Palette에서 검색할 것** | 분류 |
|---|---|---|
| 모든 `Text_...` | **`Text`** | Common |
| 모든 `Button_...` | **`Button`** | Common |
| `Bar_Health`, `Bar_Stamina`, `Bar_Experience` | **`Progress Bar`** | Common |
| `Image` (조각 아이콘 등) | **`Image`** | Common |
| `Border_Dim`, `Border_Window` | **`Border`** | Common |
| `VB_...`, `VerticalBox_...` | **`Vertical Box`** | Panel |
| `HB_...`, `HorizontalBox_...` | **`Horizontal Box`** | Panel |
| `SB_PartList`, `SB_SellList` | **`Scroll Box`** | Panel |
| `SizeBox_Window` | **`Size Box`** | Panel |
| `Overlay_Rifle` | **`Overlay`** | Panel |
| `WidgetSwitcher_Tabs` | **`Widget Switcher`** | Panel |
| 루트 | **`Canvas Panel`** | Panel |
| `Spacer` | **`Spacer`** | Primitive |

#### ⚠️ `Text` 와 `Text Box` 는 완전히 다른 위젯입니다

| Palette 이름 | 실제 클래스 | 하는 일 |
|---|---|---|
| **`Text`** | `UTextBlock` | **글자를 보여주기만** 함 → **이 문서의 `Text_...`는 전부 이것** |
| `Text Box` | `UEditableTextBox` | 사용자가 **타이핑하는 입력칸** (하얀 칸 + 커서) |

이름이 비슷해서 `Text Box`를 끌어오기 쉬운데, 그러면 로그인 폼 같은 입력창이 나옵니다.
우리 UI에는 입력칸이 하나도 없습니다. **전부 `Text`** 입니다.

### 3-4-2. 위젯별로 건드릴 항목

#### `Text`
| Details 위치 | 항목 | 설명 |
|---|---|---|
| `Content` | **`Text`** | 보여줄 글자 |
| `Appearance → Font` | **`Size`** | 제목 32 / 본문 18 / 작은 글 14 정도 |
| `Appearance` | `Color and Opacity` | 글자색 |
| `Wrapping` | **`Auto Wrap Text`** | 긴 **설명문**만 체크. **버튼 라벨은 반드시 끄기** (아래 참고) |
| 맨 위 | **`Is Variable`** | 그래프에서 `Set Text` 할 거면 체크 |

**그래프에서 글자 바꾸는 노드 고르는 법**: 정식 이름은 **`SetText (Text)`** 입니다.
빈 공간에서 검색하면 비슷한 게 잔뜩 나오니, **`Get Text_...` 의 파란 핀을 끌어다 놓고** 검색하세요.
노드 부제목이 **`Target is Text`** 면 맞게 고른 것입니다.

| 이런 부제목이면 | 정체 |
|---|---|
| **`Target is Text`** | **정답** (`UTextBlock`의 표시 이름이 `Text`라서 `Text Block`이 아님) |
| `Target is Editable Text Box` / `Multi-Line Editable Text` | 입력칸용. 아님 |
| `Target is Rich Text Block` | 서식 텍스트. 우리는 안 씀 |
| `Set Text Transform Policy` / `Overflow Policy` | 이름만 비슷한 다른 기능 |

> 헷갈리는 점: **Palette에서는 `Text`**, **변수 타입 목록에서는 `Text Block`**, **노드 부제목은 `Target is Text`** 입니다.
> 셋 다 같은 `UTextBlock`입니다.

#### `Button`
**버튼 자체에는 글자 속성이 없습니다.** Button은 **자식 하나를 담는 껍데기**라, 안에 **`Text`를 자식으로 끌어넣어야** 글자가 나옵니다.
```
Button_Start
└ Text   (Content → Text = "시작")
```

> **이 문서의 표기 주의**: 아래 위젯 트리에서 `Button_Start   "시작"` 처럼 적힌 건
> **"이 버튼의 라벨이 시작"** 이라는 메모지 속성이 아닙니다. 실제로는 항상 `Text` 자식을 하나 넣어야 합니다.

만드는 순서:
1. Palette `Button` → Hierarchy의 부모 위로 드래그 → `F2`로 이름 바꾸기
2. Palette `Text` → **Hierarchy 창에서** 그 Button 위로 드래그
   (Designer 화면에 떨구면 엉뚱한 부모로 들어가기 쉽습니다)
3. 그 Text 선택 → `Content → Text` 에 글자 입력

- **정렬은 안 건드려도 됩니다.** Button 안쪽은 기본이 Center / Center 입니다.
- 비슷한 버튼이 여러 개면 하나만 완성하고 Hierarchy에서 **우클릭 → `Duplicate`**. **자식 Text까지 같이 복사**되니 이름과 글자만 바꾸면 됩니다.
- Text를 Button 위에 못 떨구면 그 Button이 **이미 자식을 갖고 있는 것**입니다 (Button은 자식 1개만).
- `Appearance → Style` → `Normal` / `Hovered` / `Pressed` 색을 각각 지정
- Details 맨 아래 **`Events` → `On Clicked`** 옆 `+` 를 누르면 그래프에 노드가 생김
- **`Is Variable`은 Button에만** 체크하면 됩니다 (색을 바꾸거나 켜고 끌 때). 안의 Text는 글자가 안 바뀌면 체크할 필요 없습니다.

##### 버튼 글자가 찌부되거나 잘릴 때

| 어떻게 보이나 | 범인 | 고치는 법 |
|---|---|---|
| **세로로 납작** | 부모 `Size Box`의 `Height Override` | **체크 해제** (폭만 고정, 높이는 내용에 맡김) |
| **가로로 잘림** | Slot `Size = Fill` 이 글자보다 좁게 나눔 | 폰트를 18로 줄이거나 / Slot `Size`를 `Auto`로 / 창 폭을 넓히기 |
| **한 글자씩 줄바꿈** ("어려"/"움") | Text의 `Auto Wrap Text` 켜짐 | **끄기.** 버튼 라벨은 절대 켜면 안 됨 |

`Fill 1.0`은 **"글자가 얼마나 필요하든 무조건 똑같이 나눠 가져라"** 는 뜻입니다.
버튼 3개를 폭 420 창에 Fill로 넣으면 하나당 약 125px뿐이라, 폰트 24pt면 세 글자가 안 들어갑니다.
폭을 가지런히 하고 싶으면 `Fill`을 두고 **폰트를 줄이는 쪽**이 낫고, 글자 길이에 맞추고 싶으면 `Auto` + `Slot Padding Left/Right 8` 을 쓰세요.

#### `Progress Bar`
| 항목 | 값 |
|---|---|
| `Progress → Percent` | 0.0 ~ 1.0. 디자인할 때 0.7쯤 넣어두면 보기 편함 |
| `Style → Background Image → Tint` | 빈 부분 색 (어두운 회색) |
| `Style → Fill Image → Tint` | 채워진 부분 색 (체력 빨강 / 스태미나 노랑) |
| `Is Variable` | **체크** (`Set Percent` 해야 함) |

#### `Border`
- `Appearance → Brush Color` — 색 + **Alpha** (뒤 화면 어둡게 = 검정 Alpha 0.6)
- `Content → Padding` — 안쪽 여백
- 자식을 **하나만** 가질 수 있습니다. 여러 개 넣으려면 안에 Vertical Box를 하나 두세요.

### 3-5. 위젯 만들기 / 배치 기본기 (모든 위젯에 공통, 한 번만 익히면 됨)

#### 새 Widget Blueprint 만들 때 창이 두 번 뜹니다
1. `Pick Parent Class for New Widget Blueprint`
   - 보통 위젯: `User Widget`
   - 메뉴 위젯(`WBP_Computer`/`WBP_Bed`/`WBP_GameOver`/`WBP_AugmentChoice`): `All Classes` 펼쳐서 **`WBP_MenuBase`** 검색
2. `Pick Root Widget for New Widget Blueprint` → **`Canvas Panel`**

> 이미 만든 위젯의 부모를 바꾸려면: `Graph` 모드 → 툴바 **`Class Settings`** → Details → `Class Options` → **`Parent Class`**
>
> **부모 위젯(`WBP_MenuBase`)의 Designer는 비워두세요.** 자식이 자기 Root Widget을 가지면 자식 트리만 쓰이고 부모에 그려둔 건 안 나옵니다. 부모는 Event Graph만 씁니다.

#### Canvas Panel 위의 위젯은 앵커(Anchor)로 자리를 잡습니다

Canvas Panel의 자식은 **Anchor(기준점) + Position(그 기준점에서 얼마나) + Size**로 배치됩니다.
앵커를 안 잡으면 해상도가 바뀔 때 위젯이 엉뚱한 데로 갑니다.

Details 맨 위 **`Anchors`** 드롭다운에서 프리셋을 고릅니다.

| 하고 싶은 것 | 앵커 프리셋 | 그 다음 |
|---|---|---|
| 화면 전체 덮기 (배경, 큰 패널) | 오른쪽 아래 **큰 네모** (Fill) | `Offset Left/Top/Right/Bottom` 을 전부 **0** |
| 화면 정가운데 (메뉴창) | 가운데 점 | `Alignment` = **0.5, 0.5** / `Position` = 0, 0 / 크기 지정 |
| 왼쪽 위 (체력바) | 왼쪽 위 | `Alignment` = 0, 0 |
| 오른쪽 아래 (닫기 버튼) | 오른쪽 아래 | `Alignment` = **1, 1** |
| 하단 왼쪽 (꿈의 조각) | 왼쪽 아래 | `Alignment` = **0, 1** |

> **꿀팁**: 앵커 프리셋을 고를 때 **`Ctrl`을 누른 채 클릭**하면 Position까지, **`Shift`를 누른 채 클릭**하면 Alignment까지 같이 맞춰줍니다. 둘 다 원하면 `Ctrl+Shift`.

#### 크기를 고정하고 싶을 때
Canvas Panel에 직접 크기를 적는 대신 **`Size Box`** 를 하나 끼우고 `Width Override` / `Height Override`를 주면, 그 안의 Vertical Box가 알아서 그 크기 안에 들어갑니다. 버튼 개수가 바뀌어도 창 크기가 안 흔들립니다.

#### `Spacer` 쓰는 법 (이 문서에 자주 나옴)

Palette에서 `Spacer` 검색 (`Primitive` 분류) → **Vertical Box / Horizontal Box 안으로** 드래그.

> **Canvas Panel에 직접 넣으면 아무 효과 없습니다.** Canvas는 절대 좌표라 밀어낼 게 없습니다.
> **그리고 기본 `Size`가 (1, 1)이라 그냥 넣으면 1픽셀 틈이라 안 보입니다. 숫자를 꼭 바꾸세요.**

**모드 1 — 고정 간격** (문서에 `Spacer (Size Y 20)` 이라고 쓴 것)

| 어디 | 값 |
|---|---|
| `Slot (… Box Slot)` → `Size` | **`Auto`** (기본값 그대로) |
| `Appearance` → **`Size`** | 세로 박스면 **Y**에 20 / 가로 박스면 **X**에 20 |

세로 박스는 Y만, 가로 박스는 X만 의미 있습니다. 반대쪽 숫자는 티가 안 납니다.

**모드 2 — 밀어내기** (문서에 `Spacer (Fill)` 이라고 쓴 것)

| 어디 | 값 |
|---|---|
| `Slot` → `Size` | **`Fill`**, 값 1.0 |
| `Appearance` → `Size` | 안 건드려도 됨 (Fill이면 무시됨) |

```
Horizontal Box
├ Text_Title      ← 왼쪽에 붙음
├ Spacer (Fill)   ← 남는 공간을 전부 먹음
└ Text_Effect     ← 오른쪽 끝으로 밀려남
```

> **Spacer가 꼭 필요한 건 모드 2뿐입니다.** 단순히 위아래로 띄우고 싶은 거라면
> 그 위젯을 고르고 `Slot → Padding → Top 20` 을 주는 게 더 간단합니다.

### 3-6. 화면 크기 대응 — **Scale Box + Size Box**로 한 번 감싸면 끝

#### 결론부터

**화면을 꽉 채우는 위젯(HUD·인벤토리·메뉴·상점)은 전부 이 뼈대로 시작하세요.**

```
[위젯 루트]
└ Scale Box     이름: DVI_RootScale       Stretch = Scale To Fit
  └ Size Box    이름: DVI_ReferenceSize   Width Override 1920 / Height Override 1080
    └ Canvas Panel                        ← 여기서부터 자유롭게 배치
```

그리고 **Size Box의 `Slot (Scale Box Slot)` 정렬을 반드시 `Center`로 두세요.**

| Slot (Scale Box Slot) | 값 |
|---|---|
| Horizontal Alignment | **Center** |
| Vertical Alignment | **Center** |

이 뼈대를 쓰면:

- 디자이너에서 본 것과 게임 화면이 **완전히 같습니다**
- 내가 적는 숫자가 **진짜 1920 x 1080 픽셀**입니다 (배율 계산 필요 없음)
- DPI 배율이 얼마든 신경 쓸 필요가 없습니다 (Scale To Fit이 알아서 상쇄)
- 16:9가 아닌 화면에서는 위아래나 좌우에 빈 띠가 생깁니다 — **정상이고 의도한 동작**입니다

#### 정렬을 `Fill`로 두면 전부 무너집니다 (실제로 겪은 문제)

`WBP_HUD`에서 Width/Height Override는 1920/1080으로 제대로 들어가 있었는데 **Slot 정렬이 `Fill` / `Fill`** 이었습니다.

```
Scale To Fit이 하는 일
  자식이 "나 1920x1080 필요해"라고 말함 → 화면에 맞게 통째로 축소 → 가운데 배치

Slot이 Fill이면
  Scale Box가 "화면 전체 줄게" 하고 자식에게 던져버림
  → 1920x1080이라는 기준 자체가 사라짐
  → 화면 비율대로 늘어나서 체력바가 왼쪽 끝에 붙고 무기 칸이 오른쪽으로 벌어짐
```

> **증상: 디자이너에서는 멀쩡한데 게임에서만 구석 위젯들이 바깥으로 벌어진다**
> → Override 숫자 말고 **Slot 정렬**부터 보세요. 숫자는 멀쩡해 보여서 놓치기 쉽습니다.

#### 디자이너 미리보기 맞추기

1. 위젯 에디터 **`Designer`** 탭
2. 오른쪽 위 드롭다운에서 **`Screen Size` → `1920 x 1080`**
3. 그 옆 크기 모드를 **`Desired on Screen`** 으로 (`Fill Screen` 아님)

`Fill Screen`은 **에디터 패널 크기에 맞춰 늘리는 모드**라 창 크기를 바꿀 때마다 달라 보입니다. 비교 기준이 될 수 없습니다.

> 이 설정은 미리보기 전용이라 게임 실행에는 영향이 없습니다. 위젯을 새로 만들 때마다 확인하세요.

#### DPI 배율 곡선은 건드리지 마세요

참고로 우리 프로젝트의 현재 설정입니다 (`Config/DefaultEngine.ini`).

```
UIScaleRule  = ShortestSide          ← 화면 "세로"만 보고 배율을 정함
UIScaleCurve = (480, 0.667) (720, 1.0) (1080, 1.5) (1440, 2.0) (2160, 3.0)
```

엔진 기본값은 1080에서 1.0인데 **우리 프로젝트는 1.5로 바뀌어 있습니다.**

하지만 **위 뼈대를 쓰면 배율이 얼마든 상쇄되므로 곡선은 그대로 두세요.** 지금 1.0으로 되돌리면 이미 맞춰놓은 다른 위젯들이 전부 한꺼번에 틀어집니다. 고칠 거라면 UI 작업이 다 끝난 뒤에 한 번에 검수하면서 해야 합니다.

#### 뼈대를 안 쓰는 작은 위젯은

파츠 한 칸(`WBP_PartEntry`)처럼 **다른 위젯 안에 들어가는 조각**은 이 뼈대가 필요 없습니다. 부모가 이미 감싸고 있으니까요. 뼈대는 **뷰포트에 직접 올라가는 위젯에만** 씁니다.

#### 다 만든 뒤 확인 순서

드롭다운을 바꿔가며 네 번 봅니다.

| 해상도 | 봐야 할 것 |
|---|---|
| 1920 x 1080 | 기준. 여기서 디자인 |
| 1280 x 720 | **배치가 똑같아야** 함 (통째로 작아지기만) |
| 2560 x 1080 (21:9) | 좌우에 빈 띠가 생기고 내용은 그대로 |
| 3840 x 2160 (4K) | 1080과 **똑같이** 보여야 함 |

- 배치가 달라 보이면 → Slot 정렬(`Center`)과 Override(1920/1080)부터 확인
- 뼈대를 안 쓴 위젯인데 달라 보이면 → 앵커 문제입니다. 3-5 표로 돌아가세요

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

배치는 **8-0의 "가운데 창" 틀**을 그대로 씁니다 (`SizeBox_Window` 480 x 280).

```
Canvas Panel (루트)
├ Border_Dim            Anchors 전체 / Offset 0,0,0,0 / 검정 Alpha 0.6
└ SizeBox_Window        Anchors 가운데 / Alignment 0.5,0.5 / Position 0,0 / 480 x 280
  └ Border_Window
    └ VerticalBox_Content
      ├ Text_Title             "꿈에서 깨어났다"
      ├ Text_Info              난이도별 안내
      ├ Spacer                 (Size Y 20)
      └ Button_Continue
        └ Text                 "계속"
```
`Text_Info`만 `Is Variable` 체크 (난이도별 문구를 채워야 해서).

`Button_Continue` → `On Clicked`:

```
Get Owning Player → Cast To MainPlayerController → Close Menu Widget
Get Game Instance → Cast To DreamVeilGameInstance → Continue After Death
```

- `Continue After Death`가 난이도를 보고 알아서 처리합니다
  - 쉬움: 이번 판에 얻은 것까지 들고 **로비로**
  - 보통: 레벨 들어가기 전 상태로 **로비로**
  - 어려움: 전부 잃고 **메인 메뉴로** (`OpenMainMenu`)
- 그래서 `Text_Info`에 어려움일 때는 "메뉴로 돌아갑니다" 같은 안내를 넣으면 덜 당황합니다
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
>
> 값 타입은 타입 목록에서 `TextBlock`으로 검색해 **`Text Block` → `Object Reference`** 를 고르세요.
> Palette에서 끌어올 때는 `Text`지만 **변수 타입 목록에서는 클래스 이름인 `Text Block`** 으로 나옵니다.
> 글자 값인 `Text`(FText)와는 다른 것입니다.

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

> **`RefreshAll`은 Custom Event가 아니라 `Function`으로 만드세요.** 안에서 로컬 변수(`SavedOffset`)를 쓰는데,
> 로컬 변수는 함수 그래프에서만 만들 수 있습니다. 기다리는 노드가 없어서 Function으로 문제없습니다.
> 이벤트를 받는 `HandleInventoryChanged`(Custom Event)가 이 함수를 부르는 구조입니다.

```
[Function] RefreshAll
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
  Inventory → Get Parts → Get (a copy) [SelectedPartIndex] → 로컬 변수 Part (타입 WeaponPart)
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

## 8. 나머지 위젯

### 8-0. 이 세 개는 배치가 거의 같습니다 — "가운데 창" 틀

`WBP_Bed`, `WBP_AugmentChoice`, `WBP_MainMenu` 전부 **화면 가운데에 창 하나** 모양입니다.
아래 틀을 한 번 만들어 놓고 안의 내용만 바꾸면 됩니다.

```
Canvas Panel (루트)
├ Border_Dim              ← 뒤 화면을 어둡게
│   Anchors  : 전체 (오른쪽 아래 큰 네모)
│   Offset   : Left/Top/Right/Bottom 전부 0
│   Brush Color : 검정, Alpha 0.6
└ SizeBox_Window          ← 창 크기를 여기서 고정
    Anchors   : 가운데
    Alignment : 0.5 , 0.5
    Position  : 0 , 0
    Width Override / Height Override : 아래 표 참고
    └ Border_Window       (창 배경색·테두리)
      └ VerticalBox_Content   (Padding 20)
        ├ Text_Title
        ├ ... 내용 ...
        └ HorizontalBox_Buttons   (버튼들을 가로로)
```

| 위젯 | Width Override | Height Override |
|---|---|---|
| `WBP_Bed` | 480 | **끄기** |
| `WBP_AugmentChoice` | 900 | **끄기** |
| `WBP_MainMenu` | 420 | **끄기** |

> **`Height Override`는 체크 해제하세요 (폭만 고정).**
> 높이까지 고정하면 안에 든 게 그 높이를 넘을 때 Slate가 **전부 눌러서 욱여넣어** 글자가 납작해집니다.
> 폭만 고정하면 높이는 내용에 맞춰 알아서 늘어나서, 버튼을 하나 더 넣어도 안 깨집니다.

- `Border_Dim`을 먼저 넣고 그 **다음에** `SizeBox_Window`를 넣으세요. Canvas Panel은 나중에 넣은 게 위에 그려집니다.
- **버튼마다 `Text` 자식을 하나씩 넣어야 글자가 나옵니다** (3-4-2). 아래 트리에서 `Button_Go "들어가기"` 처럼 적힌 건 라벨 메모입니다.
- 버튼 사이를 띄우려면 `Spacer`를 넣고 `Size` X를 주세요.

### 8-1. `WBP_Bed` (부모: WBP_MenuBase)

```
VerticalBox_Content
├ Text_Title              "잠들기"
├ Text_Info               "자고 일어나면 다음 꿈으로 들어간다"
├ Spacer                  (Size Y 20)
└ HorizontalBox_Buttons
  ├ Button_Go    (Slot Size: Fill 1.0)
  │ └ Text                "들어가기"
  └ Button_Cancel (Slot Size: Fill 1.0)
    └ Text                "아직"
```
`Text_Info`만 `Is Variable` 체크하면 됩니다 (문구를 바꿔 넣어야 해서).

```
[Button_Go → On Clicked]
  Get Game Instance → Cast To DreamVeilGameInstance → Open Next Level
    반환값 true  → (레벨이 바뀌므로 할 일 없음)
    반환값 false → Text_Info ← "모든 꿈을 깼다"   (더 깰 레벨이 없음)

[Button_Cancel → On Clicked]
  Get Owning Player → Cast To MainPlayerController → Close Menu Widget
```
`BP_BedActor`의 `On Bed Interacted` → `Open Menu Widget (WBP_Bed)`

여유가 되면 `Get Current Level Number`, `Is Endless Unlocked`로 "다음: L3" 같은 안내를 `Text_Info`에 덧붙이세요.

### 8-2. 증강 선택 — `WBP_AugmentCard` + `WBP_AugmentChoice`

레벨업 보상으로 **3장 중 1장을 고르는 카드 화면**입니다. 되돌리기는 없고, 고르면 바로 적용되고 창이 닫힙니다.

#### 코드가 보장하는 것 (설계 근거)

| 사실 | 근거 |
|---|---|
| **선택지는 항상 정확히 3장** | `AUGMENT_CHOICE_COUNT = 3` (AugmentTypes.h) |
| **3장 미만이 나올 일이 없음** | 반복 획득 증강 4개(공/방/체/스태미나)는 `RemoveIfNotRepeatable`이 풀에 그대로 남겨서 후보가 절대 4개 아래로 안 내려감 |
| **고르는 순간 C++이 이벤트를 다시 쏨** | `SelectAugmentChoice` 안에서 `DrawNextAugmentChoices()`가 불리고, 대기 중인 보상이 있으면 `OnAugmentChoicesReady`가 또 발생 |

> 그래서 **빈 카드를 숨기는 처리(Collapsed)는 필요 없습니다.**
>
> 그리고 몬스터를 한 번에 많이 잡아서 **레벨이 두 번 오르면 3장짜리 창이 연달아 두 번 뜹니다.**
> 이건 "한 화면에서 2번 고른다"가 아니라 "고르고 나면 새 3장이 또 뜬다"는 뜻입니다.

---

#### A. `WBP_AugmentCard` — 카드 한 장 (부모: `User Widget`)

루트를 **`Size Box`** 로 만드세요 (생성할 때 Root Widget 고르는 창에서 `Size Box` 검색).
`Width Override` **280** / `Height Override` **460**

```
Size Box (280 x 460)
└ Button_Card                     ← 카드 전체가 버튼
  │   Style → Normal / Hovered / Pressed 의 Tint 알파를 0에 가깝게
  │   (테두리 이미지가 보여야 하므로 버튼 자체 배경은 숨김)
  └ Overlay
    ├ Image_Frame                 ← 테두리 그림 (전체 채움)
    └ VerticalBox_Inner           Padding (38, 68, 38, 58)  ← 테두리 안쪽으로 밀어넣기
      ├ Size Box (120 x 120)
      │ └ Image_Icon              ← 증강 그림
      ├ Spacer                    (Size Y 12)
      ├ Text_Name                 Font 22 / Justification 가운데
      ├ Spacer                    (Size Y 8)
      └ Text_Desc                 Font 14 / 가운데 / **Auto Wrap Text 체크**
```

`Is Variable` 체크: `Button_Card`, `Image_Icon`, `Text_Name`, `Text_Desc`

**변수**: `AugmentID` (타입 `EAugmentID`)
**이벤트 디스패처**: `OnCardClicked` — 입력 파라미터 `AugmentID` (`EAugmentID`) 하나

```
[Function] Setup (InAugmentID: EAugmentID, InIcon: Texture 2D)
  Set AugmentID = InAugmentID
  Image_Icon → Set Brush from Texture (InIcon)
  Text_Name  → Set Text ← Get Augment Display Name (InAugmentID)    ← static, Target 연결 불필요
  Text_Desc  → Set Text ← Get Augment Description (InAugmentID)     ← static

[Button_Card → On Clicked]
  Call OnCardClicked (AugmentID)      ← 알리고 끝. 카드는 아무것도 실행하지 않음
```

> 카드가 직접 `Select Augment Choice`를 부르면 안 됩니다.
> 고르는 순간 창이 통째로 바뀔 수 있어서, **이미 치워진 위젯 안에서 코드가 이어지는** 모양이 됩니다.

#### B. `WBP_AugmentChoice` — 카드 3장을 담는 창 (부모: **`WBP_MenuBase`**)

```
Canvas Panel
├ Border_Dim              앵커 전체 / Offset 0 / 검정 Alpha 0.75
└ VerticalBox_Root        앵커 가운데 / Alignment 0.5, 0.5 / Position 0, 0
  ├ Text_Title            "증강을 고르세요"   Font 34 / 가운데
  ├ Spacer                (Size Y 24)
  └ HorizontalBox_Cards
    ├ Card_0              ← Palette의 **User Created** 분류에서 `WBP_AugmentCard` 드래그
    ├ Card_1                 각각 Slot Size = `Auto`, Padding Left/Right 12
    └ Card_2
```

**카드 3장을 미리 배치합니다.** 항상 3장이라 실행 중에 만들 필요가 없고, 디자이너에서 눈으로 보며 맞출 수 있습니다.

**변수**

| 변수 | 타입 | 설정 |
|---|---|---|
| `AugmentIcons` | **Map**: `EAugmentID` → `Texture 2D` (Object Reference) | `Instance Editable` 체크 후 Details에서 12개 채우기 |

```
[Event Construct]
  Card_0 → Bind Event to OnCardClicked → HandleCardClicked
  Card_1 → Bind Event to OnCardClicked → HandleCardClicked     ← 셋 다 같은 커스텀 이벤트로
  Card_2 → Bind Event to OnCardClicked → HandleCardClicked

[Custom Event] Setup (Choices: EAugmentID 배열)
  Card_0 → Setup ( Choices[0] , AugmentIcons → Find(Choices[0]) )
  Card_1 → Setup ( Choices[1] , AugmentIcons → Find(Choices[1]) )
  Card_2 → Setup ( Choices[2] , AugmentIcons → Find(Choices[2]) )

[Custom Event] HandleCardClicked (AugmentID)
  Get Player Character → Cast To MainPlayerCharacter
     → Select Augment Choice (AugmentID)
     → (같은 캐릭터) Has Augment Choices ?
          true  → 아무것도 하지 않음
          false → Get Owning Player → Cast To MainPlayerController → Close Menu Widget
```

> **`true`일 때 `Close Menu Widget`을 부르면 안 됩니다.**
> 그 시점에는 이 창이 이미 치워졌고 **새 창이 떠 있어서, 닫으면 새 창이 닫힙니다.**
> (레벨이 한 번에 여러 번 올랐을 때만 생기는 경우입니다)

#### C. `BP_MainPlayerController` 연결

```
[Event On Possess] (Possessed Pawn)        ← BeginPlay가 아니라 이것. 폰이 확실히 있는 시점
  Possessed Pawn → Cast To MainPlayerCharacter
    → Bind Event to On Augment Choices Ready → HandleAugmentChoices

[Custom Event] HandleAugmentChoices (Choices)
  Open Menu Widget (Menu Widget Class: WBP_AugmentChoice)
    → (Return Value) Cast To WBP_AugmentChoice
    → Setup (Choices)
```

**창을 띄우고 내용을 채우는 일은 오직 컨트롤러만 합니다.**
위젯은 고른 것을 전달하고, 더 고를 게 없으면 자기를 닫기만 합니다.
양쪽이 다 Setup을 부르면 창이 두 겹으로 겹칩니다.

#### D. 준비할 아이콘 12개

| EAugmentID | 이름 | 반복 획득 | 가중치 |
|---|---|---|---|
| `AttackUp` | 공격력 증가 | O | 20 |
| `DefenceUp` | 방어력 증가 | O | 20 |
| `HealthUp` | 최대 체력 증가 | O | 20 |
| `StaminaUp` | 최대 스태미나 증가 | O | 12 |
| `Berserker` | 광전사 | — | 8 |
| `LastFortress` | 최후의 요새 | — | 8 |
| `ThornArmor` | 가시 갑옷 | — | 8 |
| `Vampire` | 흡혈 | — | 8 |
| `Regeneration` | 재생력 | — | 8 |
| `SlowEnemy` | 감속탄 | — | 6 |
| `AreaAttack` | 폭발탄 | — | 6 |
| `ContinuousAttack` | 독탄 | — | 6 |

위 4개(반복 획득)는 계속 뽑히니 **아이콘을 먼저 만들면** 대부분의 화면이 채워집니다.
나머지는 임시로 같은 그림을 넣어두고 나중에 교체해도 됩니다.

### 8-3. `WBP_MainMenu` (부모: `User Widget`)

부모는 `WBP_MenuBase`가 **아닙니다**. 메인 메뉴에는 플레이어도 인벤토리도 없어서 `WBP_MenuBase`가 부르는 컨트롤러 함수가 없습니다.

#### 먼저: 메인 메뉴 **레벨**이 없으면 만드세요

| 순서 | 할 것 |
|---|---|
| 1 | `Content/Maps` 에 **`Empty Level`** 로 `MainMenu` 생성 |
| 2 | **World Settings → `GameMode Override` = `GameModeBase`** (엔진 기본) |
| 3 | `Project Settings → Maps & Modes → Game Default Map` = `MainMenu` |
| 4 | 레벨 블루프린트에서 위젯 띄우기 (아래 G) |

> **2번을 빠뜨리면 메인 메뉴에 HUD가 뜨고 플레이어 캐릭터도 스폰됩니다.**
>
> C++의 `MAIN_MENU_MAP_PATH`가 **`/Game/Maps/MainMenu`** 라서 경로가 정확히 맞아야 합니다.
> 안 맞으면 하드코어에서 쓰러졌을 때 화면이 안 넘어갑니다.

#### ★ 이미 만들어둔 `WBP_MainMenu`를 이 구조로 바꾸는 법 (다시 안 만들어도 됩니다)

지금 만들어둔 난이도 화면이 그대로 `[1]`번 장이 됩니다. 껍데기만 씌우는 작업입니다.

**1단계 — 지금 있는 것을 통째로 감싸기**

1. `WBP_MainMenu` 열기
2. Hierarchy에서 지금 최상위에 있는 세로 박스(버튼들이 들어 있는 것)를 **우클릭 → `Wrap With...` → `Widget Switcher`**
3. 새로 생긴 Widget Switcher 이름을 `F2`로 **`WidgetSwitcher_Menu`**
4. 감싸진 기존 박스 이름을 **`VerticalBox_Difficulty`**

> **`Wrap With`를 쓰면 안의 내용과 그래프 연결이 그대로 유지됩니다.** 다시 배선할 필요 없습니다.

**2단계 — 첫 화면을 만들어 맨 위로**

5. `WidgetSwitcher_Menu` 안에 `Vertical Box`를 드래그 → 이름 `VerticalBox_Main`
6. Hierarchy에서 **`VerticalBox_Main`을 `VerticalBox_Difficulty` 위로 드래그**
   → 이제 Main이 `[0]`, Difficulty가 `[1]` 입니다 (자식 순서 = 인덱스)
7. Main 안에 아래 B의 버튼 4개를 넣습니다. **기존 `Button_Quit`은 여기로 옮기면 됩니다.**

**3단계 — 난이도 화면에 버튼 2개 추가**

8. `VerticalBox_Difficulty` 맨 아래에 `Button_StartRun`("시작")과 `Button_Back`("뒤로") 추가
9. **기존 `Button_Start`에 걸려 있던 `Start New Game` 연결을 `Button_StartRun`으로 옮깁니다**

> 이름이 헷갈리니 정리: 첫 화면의 `Button_Start`는 **페이지만 넘기고**,
> 난이도 화면의 `Button_StartRun`이 **실제로 게임을 시작**합니다.

**4단계 — 설명 화면**

10. `WidgetSwitcher_Menu` 안에 `Vertical Box`를 하나 더 → `VerticalBox_Help` (자동으로 `[2]`)
11. 안에 세 가지를 넣습니다 (자세한 트리는 아래 D)
    ① `HorizontalBox_HelpTabs` — 탭 버튼 4개(게임/난이도/저장/조작), 버튼마다 Text 자식
    ② `Size Box`(**Height Override 360**) → 그 안에 `WidgetSwitcher_Help` → 그 안에 `Scroll Box` 4개, 각각 Text 1개
    ③ `Button_BackFromHelp` — "뒤로"
12. 설명 글은 아래 H의 초안을 **Designer의 `Text` 칸에 직접** 붙여넣기

**5단계 — 그래프**

기존 `RefreshDifficulty` / `SetButtonHighlight`는 **손대지 않아도 됩니다.**
아래 E에서 새로 추가할 것은 `RefreshContinueButton` 함수 하나와, 페이지를 넘기는 버튼 연결뿐입니다.
`Event Construct` 맨 앞에 `Set Active Widget Index (0)`와 `RefreshContinueButton`을 끼워 넣으세요.


#### A. 화면은 한 위젯 안에서 세 장으로 넘깁니다

버튼을 누르면 **나머지가 사라지고 다음 장이 뜨는** 구조라, 위젯을 따로 만들지 않고 `Widget Switcher` 한 개로 페이지를 넘깁니다.

```
Canvas Panel
├ Image_Background          앵커 전체 / Offset 0      ← 배경 그림
├ Text_GameTitle            앵커 위쪽 가운데            "DreamVeil"
└ SizeBox_Panel             앵커 가운데 / Alignment 0.5,0.5 / Width 460 / Height Override 끄기
  └ WidgetSwitcher_Menu
    ├ [0] VerticalBox_Main          ← 첫 화면
    ├ [1] VerticalBox_Difficulty    ← 게임 시작을 누르면
    └ [2] VerticalBox_Help          ← 설명을 누르면
```

`Widget Switcher`의 자식 순서가 곧 인덱스입니다. Hierarchy에서 위에서부터 0, 1, 2.

#### B. `[0]` 첫 화면

```
VerticalBox_Main
├ Button_Start
│ └ Text            "게임 시작"
├ Button_Continue                       ← 저장이 없으면 꺼짐
│ └ Text            "이어하기"
├ Text_SaveInfo     "L3 / 보통 / Lv.7 / 09-21 02:14"   Font 14, 회색
├ Spacer            (Size Y 12)
├ Button_Help
│ └ Text            "설명"
└ Button_Quit
  └ Text            "게임 나가기"
```

`Is Variable` 체크: `Button_Continue`, `Text_SaveInfo`

#### C. `[1]` 난이도 선택 — 이미 만드신 것을 여기로 옮기면 됩니다

```
VerticalBox_Difficulty
├ Text_DiffTitle                        "난이도"
├ HorizontalBox_Difficulty
│ ├ Button_Easy    (Slot Size: Fill 1.0)
│ │ └ Text                              "쉬움"
│ ├ Button_Normal  (Slot Size: Fill 1.0)
│ │ └ Text                              "보통"
│ └ Button_Hard    (Slot Size: Fill 1.0)
│   └ Text                              "하드코어"
├ Text_DiffInfo                         고른 난이도 설명 (Auto Wrap Text 체크)
├ Spacer                                (Size Y 20)
├ Button_StartRun
│ └ Text                                "시작"
└ Button_Back
  └ Text                                "뒤로"
```

> **난이도 버튼은 난이도를 고르기만 합니다.** 실제로 게임을 시작하는 건 `Button_StartRun` 입니다.
>
> 표시 이름을 "하드코어"로 하시면 C++의 저장 요약(`GetSavedGameSummary`)도 같이 맞추세요.
> 지금 C++은 **쉬움 / 보통 / 어려움** 으로 되어 있습니다.

#### D. `[2]` 설명 — 탭 4개

```
VerticalBox_Help
├ HorizontalBox_HelpTabs
│ ├ Button_HelpGame   "게임"
│ ├ Button_HelpDiff   "난이도"
│ ├ Button_HelpSave   "저장"
│ └ Button_HelpKeys   "조작"
├ SizeBox_HelpBody        Height Override 360  ← 없으면 스크롤이 안 되고 화면 밖으로 늘어남
│ └ WidgetSwitcher_Help
│ ├ [0] Scroll Box → Text_HelpGame
│ ├ [1] Scroll Box → Text_HelpDiff
│ ├ [2] Scroll Box → Text_HelpSave
│ └ [3] Scroll Box → Text_HelpKeys
└ Button_BackFromHelp  "뒤로"
```

설명 글은 **Designer의 `Text` 칸에 직접 적으면 됩니다** (`Text` 속성은 여러 줄 입력이 됩니다).
그래프에서 채울 필요가 없으니 그 Text들은 `Is Variable`도 안 켜도 됩니다.

#### E. 그래프

```
[Event Construct]
  WidgetSwitcher_Menu → Set Active Widget Index (0)
  → RefreshContinueButton
  → RefreshDifficulty          (아래 F, 기존에 만드신 것 그대로)

[Function] RefreshContinueButton
  Get Game Instance → Cast To DreamVeilGameInstance → Has Saved Game ?
    true  → Button_Continue → Set Is Enabled (true)
            Text_SaveInfo   → Set Text ← Get Saved Game Summary
    false → Button_Continue → Set Is Enabled (false)
            Text_SaveInfo   → Set Text ← "저장된 게임이 없습니다"

-- 페이지 넘기기 --
[Button_Start        → On Clicked]  WidgetSwitcher_Menu → Set Active Widget Index (1)
[Button_Help         → On Clicked]  WidgetSwitcher_Menu → Set Active Widget Index (2)
[Button_Back         → On Clicked]  WidgetSwitcher_Menu → Set Active Widget Index (0)
[Button_BackFromHelp → On Clicked]  WidgetSwitcher_Menu → Set Active Widget Index (0)

-- 설명 탭 --
[Button_HelpGame / HelpDiff / HelpSave / HelpKeys → On Clicked]
  WidgetSwitcher_Help → Set Active Widget Index (0 / 1 / 2 / 3)

-- 실제 동작 --
[Button_Continue → On Clicked]
  Get Game Instance → Cast To DreamVeilGameInstance → Load Game From Slot
     (저장을 읽어 되돌리고 로비로 이동. 저장이 없으면 false만 돌아오고 아무 일도 안 함)

[Button_StartRun → On Clicked]
  Get Game Instance → Cast To DreamVeilGameInstance → Start New Game
     (진행도·증강·인벤토리를 전부 비우고 로비로 이동)

[Button_Quit → On Clicked]
  Quit Game        (Specific Player 비워두면 플레이어 0을 씀)
```

#### F. 난이도 강조 함수

`My Blueprint → Functions → +` → `SetButtonHighlight`

| 입력 이름 | 타입 |
|---|---|
| `TargetButton` | `Button` → **Object Reference** |
| `bSelected` | Boolean |

```
TargetButton → Set Background Color
   In Background Color ← Select 노드
        Index (Boolean) = bSelected
        True  = 노랑  (R 1.0, G 0.8, B 0.2, A 1.0)
        False = 회색  (R 0.35, G 0.35, B 0.35, A 1.0)
```

연결 순서: ① `TargetButton` 핀에서 `Set Background Color` ② `Select`를 놓고 `Index`에 `bSelected`
③ **`Select`의 `Return Value`를 먼저 `In Background Color`에 연결** ④ 그 다음 True/False 색 넣기

> ③을 먼저 해야 합니다. `Select`는 연결 전에는 타입이 안 정해져서 색 고르는 칸이 안 나옵니다.

```
[Function] RefreshDifficulty
  Get Game Instance → Cast To DreamVeilGameInstance
      → Get Difficulty → SET Current        (로컬 변수, 타입 EGameDifficulty)

  SetButtonHighlight ( Button_Easy   , Get Current == Easy   )
  SetButtonHighlight ( Button_Normal , Get Current == Normal )
  SetButtonHighlight ( Button_Hard   , Get Current == Hard   )

  Switch on EGameDifficulty ( Get Current )     ← Selection 핀에 꼭 연결할 것
    Easy   → Text_DiffInfo → Set Text  "쓰러져도 얻은 것을 전부 들고 로비로 돌아간다 / 정예 몬스터가 늦게 나온다"
    Normal → Text_DiffInfo → Set Text  "쓰러지면 이번 꿈에서 얻은 것을 잃고 로비로 돌아간다 / 정예 몬스터가 보통으로 나온다"
    Hard   → Text_DiffInfo → Set Text  "쓰러지면 끝. 저장까지 사라지고 메뉴로 돌아간다 / 정예 몬스터가 일찍부터 많이 나온다"

[Button_Easy / Normal / Hard → On Clicked]
  Get Game Instance → Cast To DreamVeilGameInstance → Set Difficulty (해당 난이도)
  → RefreshDifficulty          ← 순서 중요. Set 먼저, Refresh 나중
```

> **`Switch`의 `Selection` 옆에 드롭다운이 보이면 아무것도 연결 안 된 것입니다.** 연결되면 드롭다운이 사라집니다.
>
> `Get Difficulty`의 기본값은 `Normal`이라 메뉴를 처음 열면 "보통"이 켜진 채로 시작합니다.
>
> 로컬 변수는 **함수 그래프에서만** 만들 수 있습니다 (`My Blueprint` 패널의 `Local Variables`).

#### G. 레벨 블루프린트

MainMenu 레벨에서 `Blueprints → Open Level Blueprint`
```
Event BeginPlay
  → Create Widget (Class: WBP_MainMenu, Owning Player: Get Player Controller 0)
  → Add to Viewport
  → Get Player Controller 0 → Set Input Mode UI Only (In Widget to Focus: 만든 위젯)
  → Get Player Controller 0 → Set Show Mouse Cursor = true
```
`Create Widget`의 `Return Value`에서 선을 두 개 뽑습니다 (`Add to Viewport`, `Set Input Mode UI Only`).

#### H. 설명 글 초안 — 그대로 붙여넣고 고치세요

전부 지금 코드에서 확인한 내용입니다. 분위기·스토리만 팀에서 정한 것으로 채우면 됩니다.

**`[0] 게임`**
```
네 개의 꿈을 차례로 지나간다.

· 로비의 컴퓨터에서 파츠를 사고 강화한다.
· 침대에서 잠들면 다음 꿈으로 들어간다.
· 꿈마다 제한 시간이 있다. 시간 안에 몬스터를 전부 잡아야 깨어날 수 있다.
· 몬스터를 잡으면 꿈의 조각과 파츠가 나온다.
· 레벨이 오르면 증강 셋 중 하나를 고른다. 고른 증강은 그 판 내내 쌓인다.
· 마지막 꿈에는 보스가 있다. 보스를 넘어서면 끝없는 꿈이 열린다.
· 쓰러지면 난이도에 따라 잃는 것이 달라진다.

(※ 세계관·분위기 설명은 팀에서 정한 내용으로 채울 것)
```

**`[1] 난이도`**
```
쉬움
  쓰러져도 그 판에서 얻은 증강·파츠·꿈의 조각을 전부 들고 로비로 돌아간다.
  정예 몬스터가 늦게 나타난다.

보통
  쓰러지면 그 꿈에 들어가기 전 상태로 로비에 돌아간다.
  그 판에서 얻은 것은 사라진다.
  정예 몬스터가 보통 속도로 늘어난다.

하드코어
  쓰러지면 끝. 모든 진행과 저장이 사라지고 메인 메뉴로 돌아간다.
  정예 몬스터가 일찍부터 많이 나타난다.

시간이 다 된 경우 모두 꿈에서 깨어 로비 레벨로 돌아간다.
```

**`[2] 저장`**
```
· 로비에 돌아올 때마다 자동으로 저장된다.
· 저장 칸은 하나다. 새로 저장되면 앞의 기록을 덮어쓴다.
· 메인 메뉴의 "이어하기"로 마지막 저장 지점부터 다시 시작한다.
· 꿈 안에서는 저장되지 않는다. 꿈을 나와 로비에 도착해야 기록이 남는다.
· 하드코어에서 쓰러지면 저장도 함께 사라진다.
```

**`[3] 조작`**
```
이동            W  A  S  D
시점            마우스
달리기          Left Shift      (스태미나를 쓴다)
점프            Space
사격            마우스 왼쪽      (달리는 중에는 쏘지 않는다)
상호작용        E               (침대 · 컴퓨터)
인벤토리        I               (언제든 열 수 있고 게임은 멈추지 않는다)
무기 교체 키 1,2  (1 권총 2 소총)
```

> **무기 교체(1 / 2) 키는 아직 없습니다.** `IMC_playerController`에 `IA_EquipPistol` / `IA_EquipRifle`이
> 들어 있지 않습니다. 만들어서 넣은 뒤 이 표에 한 줄씩 추가하세요. (9장 참고)

## 9. 지금 C++에 없어서 못 만드는 것 (수요일에 같이)

| 없는 것 | 왜 필요한가 | 임시 대처 |
|---|---|---|
| **레벨 남은 시간** 조회 | HUD에 제한 시간 카운트다운 | 지금은 표시 생략. `AMainGameModeBase`에 `GetRemainingTime()` 추가 필요 |
| **메뉴가 열려 있는지** 조회 | 인벤토리와 메뉴 충돌 방지 | BP 변수 `bMenuOpen`으로 대신함 (5-1). C++에 `IsMenuOpen()`이 있으면 더 깔끔 |
| `GetWeaponInSlot`이 BP에 없음 | 무기를 슬롯으로 찾기 | `PistolWeapon` / `RifleWeapon` 변수를 직접 읽어서 해결됨 |
| **몬스터 남은 수** | HUD에 "남은 적 12" | `AMainGameModeBase`의 `AliveMonsterCount`가 private |
| **히트마커 / 피격 방향** | 맞췄다는 피드백 | 없음 |
| ~~**세이브**~~ | — | **구현 완료** (`UDreamVeilSaveGame` + GameInstance의 Save/Load 함수 5개). 로비 도착마다 자동 저장, 슬롯 1개 |
| **무기 교체 입력** | 1/2번으로 권총·소총 바꿔 들기 | **C++은 완성됨**(바인딩·핸들러 모두 있음). `IA_EquipPistol`/`IA_EquipRifle` 에셋을 만들어 IMC에 넣고 BP_MainPlayerController에 지정만 하면 됨 |
| **몬스터 처치 경험치** | 레벨업 → 증강 선택 | `AddExperience`가 치트에서만 불림. 몬스터를 잡아도 경험치가 안 들어와서 **증강 UI가 실제 플레이로는 안 뜸** |
| **몬스터 스폰** | 인벤토리·상점 테스트 | `AMonsterSpawnVolume::ExecuteSpawnActor` 분기가 비어 있음 (케디스님) |
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
| 해상도를 바꾸면 위젯이 엉뚱한 데로 감 | 앵커를 안 잡음 | 3-5의 앵커 표대로 지정 (`Ctrl+Shift` 누르고 프리셋 클릭) |
| 다른 해상도에서 글자만 작아짐 | **정상** | DPI Scaling이 하는 일. 배치만 같으면 OK (3-6) |
| 1080에서 만든 게 720에서 배치가 틀어짐 | 앵커 문제 (DPI 문제 아님) | 3-6의 확인 순서대로 네 해상도 다 보기 |
| 21:9에서 창이 왼쪽으로 쏠림 | `Alignment`가 0,0 | 가운데 창은 `Alignment` 0.5, 0.5 |
| Spacer를 넣었는데 안 벌어짐 | 기본 `Size`가 (1,1)이라 1픽셀 | `Appearance → Size`에 숫자 넣기 (세로 박스는 Y, 가로 박스는 X) |
| Spacer의 Size를 키워도 그대로임 | Slot `Size`가 `Fill`이라 자기 Size가 무시됨 | 고정 간격이면 Slot `Size`를 `Auto`로 |
| Canvas Panel에 넣은 Spacer가 무반응 | 정상 | Spacer는 Box 안에서만 동작 |
| 글자 자리에 하얀 입력칸이 생김 | `Text Box`를 끌어옴 | 지우고 **`Text`** 로 다시 (3-4-1) |
| 버튼에 글자가 안 나옴 | Button에는 글자 속성이 없음 | Button 안에 `Text`를 자식으로 넣기 |
| 긴 설명이 한 줄로 삐져나감 | 줄바꿈 꺼짐 | Text → `Wrapping → Auto Wrap Text` 체크 |
| 버튼 글자가 세로로 납작해짐 | `Size Box`의 `Height Override` | 체크 해제 (폭만 고정) |
| 버튼 글자가 가로로 잘림 | Slot `Size = Fill`이 글자보다 좁음 | 폰트 18로 줄이거나 Slot `Size`를 `Auto`로 |
| 버튼 글자가 두 줄로 접힘 | `Auto Wrap Text` 켜짐 | 버튼 라벨은 끄기 |
| Border에 두 번째 위젯이 안 들어감 | Border는 자식 1개만 | 안에 Vertical Box를 하나 두고 거기에 넣기 |
| 창이 화면 왼쪽 위에 붙어버림 | 앵커는 가운데인데 `Alignment`가 0,0 | `Alignment` = 0.5, 0.5 |
| 배경이 버튼을 덮어서 클릭이 안 됨 | 배경을 나중에 넣음 | Canvas Panel은 나중 자식이 위에 그려짐 — 배경을 맨 위로 올리기 |
| 자식 위젯에 부모 디자인이 안 보임 | 정상 | 자식이 자기 Root Widget을 가지면 부모 트리는 안 쓰임. 부모는 Graph만 (3-5) |
| 로비에서 총이 보임 | 무기 칸이 `Nothing`으로 안 바뀜 | 로비 맵 이름이 GameInstance의 `LOBBY_MAP_PATH`와 같은지 확인 |
| 로비에서 총 든 자세로 서 있음 | 애님 BP가 `Nothing`을 처리 안 함 | `Blend Poses (EWeaponSlot)`의 `Nothing Pose` 핀에 맨손 Idle 연결 |

---

## 11. 작업 순서 체크리스트

하루에 하나씩 해도 수요일 전에 끝납니다. 위에서부터 순서대로 하세요.

- [ ] `Content/UI` 폴더 만들기
- [ ] **위젯을 열 때마다: Designer 해상도를 `1920 x 1080` / `Fill Screen`으로 (3-6)**
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
  - [ ] `WBP_AugmentCard` → `WBP_AugmentChoice` (8-2 순서대로)
  - [ ] `WBP_MainMenu` — 3장짜리 Widget Switcher (첫 화면 / 난이도 / 설명)
  - [ ] `IA_EquipPistol` `IA_EquipRifle` 생성 (Digital bool) → `IMC_playerController`에 1·2키로 추가
        → `BP_MainPlayerController` Class Defaults의 `Equip Pistol Action` / `Equip Rifle Action`에 지정
        (C++은 이미 완성돼 있어 지정하는 순간 동작합니다)

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
| `Get Current Augment Choices` / `Has Augment Choices` | Pure | 고르고 난 뒤 `Has...`가 true면 새 3장이 이미 떠 있는 것 (8-2) |
| `Get Pending Augment Choice Count` | Pure | 레벨이 한 번에 여러 번 올랐을 때 **뒤에 더 기다리는 보상 수**. 화면에 안 띄움 |
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
`Start New Game` / `Open Main Menu` / `Open Next Level` / `Complete Current Level` / `Fail Current Level` / `Continue After Death`
`Is Endless Unlocked` / `Is In Level Map` / `Is In Lobby` / `Get Current Level Number` / `Is Level Unlocked (n)`
`Set Difficulty` / `Get Difficulty` / `Get Spawn Curve Scale`
`Save Game To Slot` / `Load Game From Slot` / `Has Saved Game` / `Get Saved Game Summary` / `Delete Saved Game`
저장은 로비에 도착할 때마다 자동이라 UI가 `Save Game To Slot`을 직접 부를 일은 없습니다.
메인 메뉴가 쓰는 건 `Has Saved Game`(버튼 켜기) / `Get Saved Game Summary`(표시) / `Load Game From Slot`(이어하기) 셋입니다.

### `AMainPlayerController`
`Open Menu Widget (Class)` → 만든 위젯을 돌려줍니다 / `Close Menu Widget`
Class Defaults: `Game Over Widget Class`, `HUD Widget Class`

### 구조체 `FWeaponPart` — `Break WeaponPart`
`Weapon` (권총/소총) · `Slot` (총구/탄창/조준기/개머리판/앞손잡이) · `Tier` (1~4/보스) · `EnhanceLevel` (0~5) · `bEquipped`

### enum `EWeaponSlot`
`Pistol` · `Rifle` · `Nothing`(맨손, 로비 전용 상태)
`Nothing`은 무기가 아니라 **상태**입니다. 파츠에는 절대 쓰지 마세요.
UI에서 무기 이름을 뽑을 때(`WeaponToText`) `Nothing`이 들어오면 빈 글자를 돌려주면 됩니다.
