# GGST 프랙티스 녹화 공유 모드 (GGST Practice Recording Share Mod) 초기 가이드

## 1. 프로젝트 개요
* **대상 게임**: GUILTY GEAR -STRIVE- (`H:\game\steam\steamapps\common\GUILTY GEAR STRIVE`)
* **핵심 목표**: 프랙티스(트레이닝 모드)에서 기록한 캐릭터의 행동(방어 연습용 셋업, 정역 이지선다, 프레임 트랩 등)을 **문자열(텍스트 코드) 복사/붙여넣기**로 타인과 간편하게 공유하고, 인게임 UI에서 제목과 길이 정보를 확인해 원하는 슬롯(1P/2P 더미)에 즉시 적용할 수 있는 모드 제작
* **핵심 안전 원칙**: **오직 프랙티스(Training Mode)에서만 구동**되도록 설계하여 온라인 대전 등에서 매크로나 핵으로 악용되는 것을 원천 차단

---

## 2. 핵심 설계 방향

### 2.1 인게임 프랙티스 내장 '녹화 슬롯(Slot 1~5)' 제어 방식
* **작동 원리**:
  * 길티기어 스트라이브 프랙티스 모드가 자체 보유한 **더미 녹화 슬롯(Slot 1~5) 메모리**에서 프레임별 입력 데이터와 녹화 길이(프레임 수)를 추출(Export)하거나 덮어쓰기(Import)합니다.
* **장점**:
  1. **온라인 악용 원천 차단**: 트레이닝 녹화 슬롯 메모리는 프랙티스 모드 전용 구조체이므로 온라인 매치, 아케이드, 일반 대전에서는 엔진 차원에서 작동하지 않습니다.
  2. **1P / 2P 및 방향 전환 완벽 호환**: 인게임 내장 슬롯 시스템을 그대로 활용하므로, 플레이어가 1P에서 연습하든 2P에서 연습하든, 혹은 캐릭터의 좌우 위치가 바뀌어도 앞/뒤(6/4) 가드 및 이동 방향이 자동 보정됩니다.
  3. **인게임 방어 연습 기능 연동**: 스트라이브 프랙티스의 '기상 리버설 재생', '가드 후 리버설 재생', '여러 슬롯 랜덤 재생' 기능을 그대로 사용할 수 있습니다.
* **이중 안전장치 (Hard-Lock)**:
  * 메모리 구조적 차단에 더해, 모드 코드 자체에서도 현재 게임 상태(`GameState` / `GameMode`)가 프랙티스 모드인지 매 프레임 검사하여, 프랙티스 모드가 아닐 경우 UI 렌더링 및 메모리 읽기/쓰기 기능을 전면 비활성화합니다.

---

### 2.2 클립보드 문자열(코드) 공유 및 검증 프리뷰 시스템
* **문자열 복사/붙여넣기 공유 포맷**:
  * 별도의 파일을 주고받을 필요 없이 디스코드, 오픈채팅, 커뮤니티 게시글 등에 **텍스트 문자열 하나로 복사/붙여넣기**할 수 있는 포맷을 사용합니다.
  * 프레임별 입력 데이터는 연속된 동일 입력을 압축(RLE/압축 알고리즘)한 뒤 메타데이터와 함께 Base64 텍스트 코드로 변환하여 문자열 길이를 최소화합니다.
  * 예시 코드 형태: `GGSTREC1:[솔_배드가이|구석_근S_정역셋업|145f]:eJxLZmBg...`
* **붙여넣기 시 인식 및 확인 팝업 ("이거 맞아?" 검증 단계)**:
  * 유저가 문자열을 복사한 뒤 인게임 UI에서 **[클립보드에서 붙여넣기]**를 누르면, 즉시 적용하지 않고 먼저 데이터를 파싱해 **인식 정보 확인 창**을 띄웁니다.
  * **표시 정보**:
    * **제목 (Title)**: 예) `솔 근S 가드 후 정역 이지 1`
    * **기록 캐릭터 (Character)**: 예) `Sol Badguy (솔 배드가이)`
    * **녹화 길이 (Duration)**: 예) `145 프레임 (2.41초)`
    * **작성자 메모 / 권장 위치**: 예) `구석 시작 권장`
  * 위 정보를 명확히 보여준 뒤 **"인식된 녹화 정보가 맞습니까?" → `[맞음 (저장 및 슬롯 적용)]` / `[취소]`** 버튼을 통해 안전하게 확인 후 적용합니다.
* **인게임 로컬 목록(보관함) 지원**:
  * 한 번 불러온 녹화나 내가 직접 만든 녹화는 로컬 보관함 파일(`Recordings.json`)에 자동 저장되어, 이후에는 문자열을 다시 붙여넣지 않아도 인게임 리스트에서 **제목과 길이**만 보고 원하는 슬롯(1~5번)에 즉시 갈아끼울 수 있습니다.

---

### 2.3 인게임 오버레이 UI 및 실시간 단축키(Keybind) 변경
* **DirectX 11 ImGui 오버레이 창**:
  * 알트탭(Alt+Tab)으로 게임 창을 나갈 필요 없이 게임 화면 위에 반투명 메뉴 창을 띄워 마우스/키보드로 조작합니다.
* **단축키(Toggle Key) 자유 설정 및 즉각 반영**:
  * **기본 단축키**: `F12` (또는 초기 설정값)로 오버레이 창 열기/닫기
  * **즉시 변경 기능**: 다른 모드(프레임 뷰어 등)나 스팀 기본 키와 겹칠 경우를 대비해, 열린 오버레이 창 상단에 **`[열기 단축키 변경: 현재 F12]`** 버튼을 배치합니다.
  * 버튼을 누르면 *"사용할 키를 눌러주세요... (ESC: 취소)"* 상태가 되며, 원하는 키(예: `F8`, `Insert`, `Home`, `PageUp` 등)를 누르는 즉시 해당 키로 단축키가 변경됩니다.
  * 변경된 단축키는 모드 폴더 내 설정 파일(`config.ini`)에 즉시 저장되어 게임을 재실행해도 그대로 유지됩니다.

---

### 2.4 의존성 포함 올인원(All-in-One) 배포 및 폴더 구조
모드를 다른 사람에게 배포할 때 상대방이 UE4SS 같은 선행 모드를 따로 찾아 설치하느라 헤매지 않도록, **프로젝트 폴더 내에 의존성 파일들을 하나로 묶은 배포용 패키지 구조**를 유지합니다.

```text
g:\antigravity Project\ggst record\
├── PROJECT_GUIDE.md             # 프로젝트 방향성 및 설계 가이드 문서
├── src\                         # 모드 C++ 소스 코드 및 ImGui/후킹 모듈
├── build\                       # 컴파일 빌드 산출물 임시 폴더
└── dist\                        # [공유/배포용 패키지 폴더]
    ├── All_In_One_Package\      # UE4SS 미설치 유저용 (Win64 폴더에 통째로 복사)
    │   ├── dwmapi.dll
    │   └── ue4ss\
    │       ├── UE4SS.dll
    │       ├── UE4SS-settings.ini
    │       └── Mods\
    │           ├── mods.txt
    │           └── GGSTPracticeShare\
    │               ├── dlls\
    │               │   └── main.dll
    │               ├── config.ini
    │               └── Recordings.json
    │
    └── Mod_Only_Package\        # 이미 UE4SS(프레임 뷰어 등)가 깔린 유저용
        └── GGSTPracticeShare\
            ├── dlls\
            │   └── main.dll
            ├── config.ini
            └── Recordings.json
```

* **`All_In_One_Package`**: 모드를 처음 쓰는 사람에게 공유할 때 사용. 압축을 풀어 `GUILTY GEAR STRIVE\RED\Binaries\Win64`에 그대로 넣기만 하면 UE4SS와 모드가 한 번에 정상 구동됩니다.
* **`Mod_Only_Package`**: 이미 `UE4SS`를 사용 중인 사람에게 공유할 때 사용. `ue4ss\Mods` 폴더 안에 폴더만 넣으면 기존 모드들과 충돌 없이 함께 구동됩니다.

---

## 3. 향후 제작 진행 시 작업 단계 (Roadmap)

실제 제작에 착수할 경우 아래의 5단계로 나누어 진행합니다.

### 1단계: 프랙티스 모드 및 녹화 슬롯 메모리 구조 분석
* 현재 설치된 `GGST-Win64-Shipping.exe` 및 기존 C++ 모드(`GGSTFrameViewer`)의 구조를 참고하여 다음 메모리 오프셋/시그니처(AOB)를 확보합니다.
  1. 현재 게임 모드가 **프랙티스(Training Mode)**인지 판별하는 상태값
  2. 현재 매치의 1P / 2P 캐릭터 ID 정보
  3. 프랙티스 모드의 **녹화 슬롯(Slot 1~5) 입력 버퍼 주소, 프레임 길이 변수, 슬롯 상태값**

### 2단계: 프로젝트 빌드 환경 및 배포 폴더(`dist`) 세팅 [완료]
* C++ DLL 모드 프로젝트(CMake / MSVC x64)를 `src/`에 구성 완료.
* ImGui, MinHook, miniz, nlohmann/json 외부 의존성을 자체 포함하여 외부 추가 라이브러리 없이 정적 런타임(`/MT`)으로 단일 `main.dll` 빌드 구축 완료.
* 게임 내 UE4SS 런타임 파일(`dwmapi.dll`, `UE4SS.dll`, `UE4SS-settings.ini`, `mods.txt`)을 연동한 `dist/` 올인원 및 단독 패키징 구조 구축 완료.

### 3단계: DX11 ImGui 오버레이 및 실시간 단축키 변경 구현 [완료]
* 게임 화면 내 DirectX 11 SwapChain Present & ResizeBuffers 후킹 및 WndProc 서브클래싱 연결 완료.
* 기본 토글 단축키(`F12`)로 오버레이 창을 열고 닫을 수 있으며, 인게임 상단 `[열기 단축키 변경]` 버튼 클릭 후 원하는 키를 누르면 즉시 단축키가 변경되고 `config.ini`에 영구 보존됨.
* 윈도우 기본 한글 폰트(`malgun.ttf` / `gulim.ttc`) 자동 로드 지원.

### 4단계: 녹화 슬롯 추출/주입 및 문자열 공유(프리뷰 확인) 구현 [완료]
* 인게임 슬롯(1~5번) 메모리 읽기, 쓰기, 초기화 기능 및 32명 캐릭터 ID/시선 방향 실시간 매핑 구현 완료.
* 프랙티스 모드 판별(`GameMode == 8 || 19`)을 통한 타 모드/온라인 악용 원천 차단(Hard-Lock) 적용 완료.
* zlib(miniz) deflate 압축 + Base64 인코딩을 적용한 초경량 공유 코드(`GGSTREC1:...`) 생성 및 파싱 완료.
* 클립보드 붙여넣기 시 **"제목 / 캐릭터(현재 매치 일치 여부) / 길이(초) / 메모 / 슬롯 선택"**을 직관적으로 확인하고 주입하는 **"이거 맞아?" 인식 정보 확인 모달창** 구현 완료.
* 한 번 불러온 녹화나 직접 저장한 녹화를 관리할 수 있는 로컬 보관함(`Recordings.json`) 및 원클릭 슬롯 주입 UI 연동 완료.

### 5단계: 인게임 테스트 및 배포 패키지 최종 점검 [진행 준비]
* 프랙티스 모드 진입 시 정상 작동 여부, 온라인/타 모드 진입 시 자동 비활성화(Hard-Lock) 작동 여부, 단축키 변경 및 공유 문자열 호환성을 검증하고 배포용 압축 패키지를 완성합니다.

---

## 4. [1단계 완료] 메모리 구조 및 오프셋 분석 명세 (Technical Specification)

현재 설치된 `GGST-Win64-Shipping.exe` (2026-07-02 빌드) 및 `GGSTFrameViewer` 바이너리를 정밀 역공학하여 도출한 **실제 메모리 주소(RVA), 시그니처(AOB), 데이터 구조체** 명세입니다. 2~4단계 구현 시 아래 값과 구조체를 그대로 사용합니다.

### 4.1 바이너리 보호(SteamStub 3.1) 및 초기화 주의사항
* `GGST-Win64-Shipping.exe`의 `.text` 섹션(`RVA 0x1000 ~ 0x3668E00`)은 디스크 상에서 **SteamStub 3.1 (AES-256-CBC)**로 암호화되어 있으며, 게임 실행 직후 엔트리포인트(`.bind` 섹션 `RVA 0x57C5310`)에서 메모리 복호화가 수행된 뒤 원래 엔트리포인트(`RVA 0x35DEAD0`)로 점프합니다.
* **주의**: DLL이 로드되는 `DllMain` 시점에는 아직 `.text` 섹션이 복호화되기 전일 수 있으므로, 시그니처 스캔이나 후킹은 반드시 **UE4SS의 `on_unreal_init()` 콜백 시점** 또는 `*(uint8_t*)(ImageBase + 0x1000) == 0xB8` (복호화 완료 확인) 이후에 수행해야 합니다.

---

### 4.2 핵심 전역 포인터 및 RVA (`ImageBase = GetModuleHandleW(L"GGST-Win64-Shipping.exe")`)

| 심볼 명칭 | RVA 오프셋 | 타입 | 설명 및 용도 |
| :--- | :--- | :--- | :--- |
| **`GWorld`** | `0x529B2F0` | `UWorld**` | 현재 언리얼 월드 포인터 (`*GWorld + 0x130` = `AREDGameState_Battle*`) |
| **`GGameCommon`** | `0x50083F0` | `UREDGameCommon**` | 게임 공통 매니저 (`GameMode` 및 `1P/2P 진영` 확인용) |
| **`GSaveDataManager`** | `0x4FE7810` | `uintptr_t*` | 트레이닝 녹화 슬롯(Slot 1~5) 및 프리셋이 저장된 베이스 포인터 |
| **`GTrainingManager`** | `0x4AD32F0` | `TrainingManager` (정적 구조체) | 프랙티스 모드 런타임 제어 구조체 (포인터가 아닌 구조체 시작 주소) |

* **AOB 시그니처 (패치 대비 백업용)**:
  * `GWorld` 추출 (`RVA 0x6081BD`):
    * 패턴: `0F 2E ?? 74 ?? 48 8B 1D ?? ?? ?? ?? 48 85 DB 74` (`+8` 위치의 32비트 상대 오프셋 해석: `addr + 0xC + *(int32_t*)(addr + 0x8)`)
  * `GSaveDataManager` 및 녹화 슬롯 접근 함수 (`GetCurrentRecordingSlotStructPtr`, `RVA 0xBD7CF0`):
    * 패턴: `48 63 43 04 48 69 C8 28 15 00 00 48 8B 05 ?? ?? ?? ?? 48 05 D4 84 02 00`

---

### 4.3 프랙티스 모드 판별 (온라인 악용 방지 Hard-Lock 기준)

1. **`GGameCommon`을 통한 게임 모드 판별 (`RVA 0x11002F0` / `0xBD33A6` 검증 완료)**:
   ```cpp
   uintptr_t gameCommon = *(uintptr_t*)(ImageBase + 0x50083F0);
   if (!gameCommon) return false;
   uint8_t gameMode = *(uint8_t*)(gameCommon + 0x2F0);
   ```
   * `gameMode == 8` (`0x08`): **오프라인 프랙티스 (Training Mode)**
   * `gameMode == 19` (`0x13`): **트레이닝 대기 모드 (Wait in Training)**
   * *(참고: `23 (0x17)` = 리플레이 시어터, `26 (0x1A)` = 콤보 메이커)*
   * **Hard-Lock 조건**: `gameMode == 8` (필요시 `19` 포함 여부 선택 가능, 기본은 순수 오프라인 프랙티스인 `8`만 허용하거나 `8, 19`만 허용)이고, `*GWorld` 및 `*(uintptr_t*)(*GWorld + 0x130)` (`AREDGameState_Battle*`)이 유효할 때만 모드 활성화.
2. **현재 플레이어 진영(1P / 2P) 확인**:
   ```cpp
   uint8_t mainPlayerSide = *(uint8_t*)(gameCommon + 0x2F4); // 0 = 1P(왼쪽), 1 = 2P(오른쪽)
   ```

---

### 4.4 프랙티스 내장 녹화 슬롯 (Slot 1 ~ Slot 5) 메모리 구조체

* **슬롯 배열 베이스 주소**:
  ```cpp
  uintptr_t saveDataBase = *(uintptr_t*)(ImageBase + 0x4FE7810);
  // 슬롯 인덱스 i = 0 ~ 4 (인게임 Slot 1 ~ Slot 5)
  uintptr_t slotAddr = saveDataBase + 0x284D4 + (i * 0x1528);
  ```
* **슬롯 구조체 정의 (`sizeof(GGSTRecordingSlot) == 0x1528` = `5,416 Bytes`)**:
  (`RVA 0x11D6D30` 전체 슬롯 초기화, `RVA 0xBD5F61` 녹화 저장, `RVA 0xBD1165` 재생 루프 역공학 결과 100% 일치 검증)
  ```cpp
  #pragma pack(push, 1)
  struct GGSTRecordingSlot {
      uint8_t  bHasData;            // +0x00: 데이터가 있으면 1, 비어있으면 0
      uint8_t  Pad01[3];            // +0x01: 패딩 (0)
      int32_t  RecordedFacingDir;   // +0x04: 녹화 시점 더미의 시선 방향 (0 = 오른쪽/1P방향, 1 = 왼쪽/2P방향)
      int32_t  TotalFrameCount;     // +0x08: 마지막 녹화 프레임 인덱스 (비어있을 때 -1 / 0xFFFFFFFF, 유효할 때 > 4)
      int32_t  StartOffsetFrame;    // +0x0C: 시작 오프셋 프레임 (기본 0, 실제 재생 프레임 길이 = TotalFrameCount - StartOffsetFrame)
      uint16_t FrameInputs[2700];   // +0x10 ~ +0x1527: 최대 2700프레임(45.0초 @ 60fps) 분량의 프레임별 입력 비트마스크 (0x1518 바이트)
  };
  #pragma pack(pop)
  static_assert(sizeof(GGSTRecordingSlot) == 0x1528, "Slot size must be 0x1528 bytes");
  ```
* **재생 길이(Duration) 및 인덱싱 규칙**:
  * 인게임 UI(`RVA 0x11E2C11`) 및 재생 루프(`RVA 0xBD0FC0`) 기준:
    * **실제 유효 프레임 길이**: `int32_t duration = slot.TotalFrameCount - slot.StartOffsetFrame;`
    * **슬롯 비어있음 판정(`RVA 0xBCBE90`)**: `TotalFrameCount <= 4` 이거나 `FrameInputs[0 .. TotalFrameCount - 2]`가 모두 `0`이면 빈 슬롯으로 취급합니다.
    * 외부에서 슬롯에 데이터를 **주입(Import)**할 때는:
      * `bHasData = 1`
      * `RecordedFacingDir = 0` (또는 녹화 데이터에 저장된 기준 방향 `0` / `1`)
      * `StartOffsetFrame = 0`
      * `TotalFrameCount = 프레임_수` (최소 5 이상 권장, 5 미만이면 끝에 중립 프레임 패딩)
      * `FrameInputs[0 .. 프레임_수 - 1]`에 입력 데이터를 채우고 나머지는 `0`으로 초기화 (`memset`)

---

### 4.5 16비트 프레임 입력 비트마스크 (`uint16_t`) 규격

* **하위 4비트 (`input & 0x000F`) — 방향키 (좌우 반전 함수 `RVA 0xD37260` 기준)**:
  * `0x0`: 중립 (5)
  * `0x1`: 위 (8)
  * `0x2`: 아래 (2)
  * `0x4`: 왼쪽 (4) ↔ 반전 시 `0x8` (오른쪽, 6)
  * `0x5`: 왼쪽 위 (7) ↔ 반전 시 `0x9` (오른쪽 위, 9)
  * `0x6`: 왼쪽 아래 (1) ↔ 반전 시 `0xA` (오른쪽 아래, 3)
  * `0x8`: 오른쪽 (6) ↔ 반전 시 `0x4` (왼쪽, 4)
  * `0x9`: 오른쪽 위 (9) ↔ 반전 시 `0x5` (왼쪽 위, 7)
  * `0xA`: 오른쪽 아래 (3) ↔ 반전 시 `0x6` (왼쪽 아래, 1)
* **상위 12비트 (`input & 0xFFF0`) — 공격/특수 버튼 비트마스크**:
  * P, K, S, HS, D, 대시 매크로, 로망 캔슬, 폴트리스 디펜스, 도발 등의 버튼 입력 비트가 그대로 저장됨 (좌우 방향과 무관하게 그대로 유지)

---

### 4.6 배틀 중 1P / 2P 캐릭터 ID 및 방향 조회 오프셋

녹화 데이터를 추출(Export)하거나 프리뷰 창("이거 맞아?")에서 현재 캐릭터와 비교할 때 사용합니다. (`RVA 0xBD06B3` 및 `RVA 0xBD42CB` 검증 완료)

```cpp
uintptr_t world = *(uintptr_t*)(ImageBase + 0x529B2F0);
uintptr_t gameState = world ? *(uintptr_t*)(world + 0x130) : 0;
uintptr_t battleObjMgr = gameState ? *(uintptr_t*)(gameState + 0xC20) : 0; // asw_engine
```
* **플레이어별 오프셋 (`PlayerStride = 0xADB80`)**:
  * **1P 캐릭터 ID (`uint8_t`)**: `*(uint8_t*)(battleObjMgr + 0x274DDE)`
  * **2P 캐릭터 ID (`uint8_t`)**: `*(uint8_t*)(battleObjMgr + 0x32295E)` (`0x274DDE + 0xADB80`)
  * **1P 시선 방향 (`int32_t`)**: `*(int32_t*)(battleObjMgr + 0x27B0AC)` (`0` = 오른쪽, `1` = 왼쪽)
  * **2P 시선 방향 (`int32_t`)**: `*(int32_t*)(battleObjMgr + 0x328C2C)` (`0x27B0AC + 0xADB80`)

* **캐릭터 ID 테이블 (`0 ~ 31`, `Raw 0x38BBEB0` 기준)**:
  | ID | 코드 | 캐릭터명 | ID | 코드 | 캐릭터명 |
  | :-: | :-: | :--- | :-: | :-: | :--- |
  | 0 | `SOL` | 솔 배드가이 (Sol Badguy) | 16 | `JKO` | 잭오 (Jack-O') |
  | 1 | `KYK` | 카이 키스크 (Ky Kiske) | 17 | `COS` | 해피 케이오스 (Happy Chaos) |
  | 2 | `MAY` | 메이 (May) | 18 | `BKN` | 바이켄 (Baiken) |
  | 3 | `AXL` | 액슬 로우 (Axl Low) | 19 | `TST` | 테스타먼트 (Testament) |
  | 4 | `CHP` | 치프 자너프 (Chipp Zanuff) | 20 | `BGT` | 브리짓 (Bridget) |
  | 5 | `POT` | 포템킨 (Potemkin) | 21 | `SIN` | 신 키스크 (Sin Kiske) |
  | 6 | `FAU` | 파우스트 (Faust) | 22 | `BED` | 베드맨? (Bedman?) |
  | 7 | `MLL` | 밀리아 레이지 (Millia Rage) | 23 | `ASK` | 아스카 R# (Asuka R#) |
  | 8 | `ZAT` | 자토-ONE (Zato-1) | 24 | `JHN` | 조니 (Johnny) |
  | 9 | `RAM` | 램리썰 밸런타인 (Ramlethal) | 25 | `ELP` | 엘페르트 밸런타인 (Elphelt) |
  | 10 | `LEO` | 레오 화이트팽 (Leo Whitefang) | 26 | `ABA` | 아바 (A.B.A) |
  | 11 | `NAG` | 나고리유키 (Nagoriyuki) | 27 | `SLY` | 슬레이어 (Slayer) |
  | 12 | `GIO` | 지오바나 (Giovanna) | 28 | `DZY` | 퀸 디지 (Queen Dizzy) |
  | 13 | `ANJ` | 미토 안지 (Anji Mito) | 29 | `VEN` | 베놈 (Venom) |
  | 14 | `INO` | 이노 (I-No) | 30 | `UNI` | 유니카 (Unika) |
  | 15 | `GLD` | 골드루이스 (Goldlewis) | 31 | `LUC` | 루시 (Lucy) |

