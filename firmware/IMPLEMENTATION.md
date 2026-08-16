# 구현 현황 및 코드 구조

---

## 전체 아키텍처

```
Raspberry Pi                    ESP8266 (SmallTV Ultra)
┌─────────────────────┐  HTTP  ┌───────────────────────────────────────┐
│ pi/draw_client.py   │ ─────► │ main.cpp                              │
│ (드로잉 커맨드 전송) │        │  ├── DisplayMode: clock / draw / jpeg │
└─────────────────────┘        │  ├── HTTP 서버 (포트 80)              │
                                │  └── 1초 클럭 Ticker                 │
                                └───────────────────────────────────────┘
                                    │         │          │         │
                               display   draw_cmd  http_server  clock_theme
                               (TFT래퍼)  (JSON파서)  (API서버)   (기본시계)
                                    │                   │
                               TFT_eSPI             filesystem
                               ST7789V              LittleFS
                               (240×240)         jpeg_display
```

---

## 디스플레이 모드

ESP8266은 세 가지 모드를 가지며, `http_server`가 상태를 관리합니다.

```
부팅
 └─ clock 모드 (기본)
     ├─ POST /draw  → draw 모드 (Pi가 명령 전송 시)
     │   └─ timeout 만료 or GET /mode?set=clock → clock 모드로 복귀
     └─ POST /display or /doUpload → jpeg 모드
```

| 모드 | 상수 | 동작 |
|------|------|------|
| clock | `MODE_CLOCK` | 1초마다 `clockThemeRender()` 호출 |
| draw | `MODE_DRAW` | `/draw`로 받은 명령 렌더링, 이후 대기 |
| jpeg | `MODE_JPEG` | `/display`로 받은 JPEG 표시, 이후 대기 |

---

## 모듈별 구현 상세

### 1. `config.h` — 전역 상수

| 상수 | 값 | 설명 |
|------|----|------|
| `FW_VERSION` | `"Custom-V1.0.0"` | OTA 폼에 표시 |
| `WIFI_AP_NAME` | `"GeekMagic-Setup"` | 첫 실행 AP 이름 |
| `NTP_SERVER` | `"pool.ntp.org"` | NTP 서버 |
| `NTP_OFFSET_SEC` | `32400` (UTC+9) | KST |
| `DISPLAY_W/H` | `240` | 화면 해상도 |
| `DEFAULT_BRIGHTNESS` | `200` | 기본 백라이트 |
| `DEBUG` | `1` | Serial 로그 ON/OFF |

---

### 2. `display.cpp / display.h` — TFT 드라이버 래퍼

TFT_eSPI를 감싸서 상위 모듈이 하드웨어를 직접 다루지 않도록 추상화합니다.  
`extern TFT_eSPI tft` 를 통해 `draw_cmd.cpp`에서도 접근 가능합니다.

| 함수 | 역할 |
|------|------|
| `displayInit()` | TFT 초기화, rotation=0, 백라이트 PWM 켜기 |
| `displayFill(colour)` | 전체 화면 단색 채우기 |
| `displayText(x,y,text,colour,size)` | 좌표 지정 텍스트 |
| `displayTextCentre(x,y,w,text,…)` | 너비 w 안에서 가운데 정렬 |
| `displayTextRight(x,y,text,…)` | 우측 정렬 |
| `displayHLine(x,y,len,colour)` | 수평선 |
| `displayVLine(x,y,len,colour)` | 수직선 |
| `displayRect(x,y,w,h,colour)` | 채운 사각형 |
| `displayRoundRect(…)` | 둥근 사각형 외곽선 |
| `displayLine(x1,y1,x2,y2,colour)` | 직선 |
| `displayCircle(x,y,r,colour)` | 원 외곽선 |
| `displayCircleFill(x,y,r,colour)` | 채운 원 |
| `displaySetBrightness(level)` | 백라이트 PWM (0–255) |
| `displayBootSplash()` | 부팅 로고 화면 |

> **베젤 제약**: 케이스 물리 구조상 **y < 75 영역이 가려짐**.  
> 모든 콘텐츠는 **y ≥ 75**부터 배치.

**구현 상태: 완료**

---

### 3. `draw_cmd.cpp / draw_cmd.h` — JSON 드로잉 커맨드

`POST /draw`로 받은 JSON을 파싱해 TFT 프리미티브 함수를 호출합니다.

#### 처리 흐름

```
JSON body 수신
 └─ deserializeJson() (ArduinoJson v7)
     ├─ "clear": true → displayFill(parseColor("bg"))
     └─ "elements" 배열 순회
         ├─ type="text"   → displayText / displayTextCentre / displayTextRight
         ├─ type="rect"   → displayRect / tft.drawRect
         ├─ type="circle" → displayCircle / displayCircleFill
         ├─ type="line"   → displayLine
         ├─ type="hline"  → displayHLine
         └─ type="vline"  → displayVLine
```

#### 색상 파싱 (`parseColor`)

- 문자열 이름: `white` `black` `red` `green` `blue` `cyan` `yellow` `orange` `magenta` `grey`
- Hex 문자열: `"#RRGGBB"` → `tft.color565(r, g, b)`

| 함수 | 역할 |
|------|------|
| `drawExecute(json)` | JSON 문자열 전체를 파싱해 렌더링. 성공 시 true 반환 |

**구현 상태: 완료 (테스트 통과)**

---

### 4. `http_server.cpp / http_server.h` — HTTP API 서버

포트 80, `ESP8266WebServer` 기반. 디스플레이 모드 상태(`DisplayMode`)를 관리합니다.

#### 모드 상태 변수

```cpp
static DisplayMode   _mode          = MODE_CLOCK;
static unsigned long _modeSetAt     = 0;
static unsigned long _modeTimeoutMs = 0;   // 0 = timeout 없음
```

#### 엔드포인트

| 메서드 | 경로 | 동작 |
|--------|------|------|
| GET | `/` | 상태 JSON `{"fw":…,"mode":…,"brt":…,"heap":…}` |
| GET | `/mode?set=clock` | `MODE_CLOCK`으로 전환, clockThemeInit() 호출 |
| POST | `/draw` | JSON 파싱 → drawExecute() → MODE_DRAW |
| POST | `/display` | JPEG 수신 → LittleFS 저장 → jpegDisplayFile() → MODE_JPEG |
| POST | `/doUpload` | `/display`와 동일 (레거시 호환) |
| GET | `/set?brt=N` | 백라이트 밝기 조절 |
| GET/POST | `/update` | OTA 펌웨어 업데이트 |

#### 주요 공개 함수

| 함수 | 역할 |
|------|------|
| `httpServerInit()` | 라우트 등록 + 서버 시작 |
| `httpServerHandle()` | loop()에서 매 반복 호출 |
| `httpGetDisplayMode()` | 현재 모드 반환 |
| `httpCheckModeTimeout()` | timeout 만료 시 MODE_CLOCK 복귀, true 반환 |
| `httpGetBrightness()` | 현재 밝기 반환 |

**구현 상태: 완료**

---

### 5. `filesystem.cpp / filesystem.h` — LittleFS

JPEG 이미지 저장에 사용합니다. (설정 파일 저장은 현재 미사용)

| 함수 | 역할 |
|------|------|
| `fsInit()` | LittleFS 마운트 (실패 시 자동 포맷) |
| `fsSaveUpload(data, len)` | `/image/weather_clock.jpg`에 저장 |
| `fsExists(path)` | 파일 존재 확인 |
| `fsDelete(path)` | 파일 삭제 |
| `fsFileSize(path)` | 파일 크기 반환 |

**구현 상태: 완료**

---

### 6. `jpeg_display.cpp / jpeg_display.h` — JPEG → TFT 스트리밍

JPEGDEC 라이브러리로 JPEG를 디코딩하고 TFT에 직접 스트리밍합니다.  
프레임버퍼(115KB)를 사용하지 않아 힙 소비를 최소화합니다.

```
LittleFS → malloc(파일크기) → JPEGDEC.openRAM()
 → jpegDraw 콜백 → tft.pushImage() → free()
```

| 함수 | 역할 |
|------|------|
| `jpegDisplayFile(path)` | LittleFS 경로의 JPEG를 TFT 전체에 표시 |

**구현 상태: 완료**

---

### 7. `clock_theme.cpp / clock_theme.h` — 기본 시계

`MODE_CLOCK` 상태에서 1초마다 렌더링됩니다.  
변경된 숫자 자리만 덮어써서 깜빡임을 최소화합니다.

#### Theme 4 레이아웃 (현재 활성)

```
y=0   ────── (베젤에 가려짐)
y=76  ──────  HH:MM   (size=4, CYAN)
y=152 ──────  :SS     (size=2, GREY)
y=183 ──────  구분선  (GREY)
y=190 ──────  2025.08.15 FRI  (size=2, WHITE)
y=240 ──────
```

| 함수 | 역할 |
|------|------|
| `clockThemeInit()` | 캐시 초기화 → 다음 틱에 전체 재렌더링 |
| `clockTimeValid()` | NTP 동기화 완료 여부 |
| `clockThemeRender(theme)` | 1초 틱마다 호출되는 렌더러 |

> TFT_eSPI 내장 폰트는 ASCII 전용. 한글은 깨지므로 요일은 영문(SUN–SAT) 사용.

**구현 상태: 완료**

---

### 8. `main.cpp` — 진입점

#### setup() 초기화 순서

```
1. Serial 초기화 (DEBUG=1)
2. displayInit() → displayBootSplash()
3. fsInit()
4. wifiSetup() — WiFiManager (저장 SSID 있으면 즉시 연결, 없으면 AP 모드)
5. NTP 시작 및 첫 동기화 (KST UTC+9)
6. httpServerInit()
7. 1초 Ticker 등록 → clockThemeInit()
8. displayFill(BLACK) → 시계 대기
```

#### loop() 처리 순서

```
1. httpServerHandle()       — HTTP 요청 처리
2. ESP.wdtFeed()            — 워치독 리셋
3. syncPosixTime()          — NTP 재동기 (60초 간격)
4. 모드 전환 감지
   └─ MODE_CLOCK로 복귀 시 clockThemeInit() + 화면 클리어
5. httpCheckModeTimeout()   — draw/jpeg timeout 체크
6. _clockTick 확인 (1초마다)
   └─ MODE_CLOCK → clockThemeRender()
      MODE_DRAW / MODE_JPEG → 아무것도 하지 않음 (화면 유지)
```

---

## 메모리 사용량

| 항목 | 사용 | 한도 |
|------|------|------|
| Flash | **~48%** (503KB) | 1,044KB |
| RAM (정적) | ~72% (59KB) | 81KB |
| 런타임 힙 | ~17KB | — |

### 메모리 절약 전략

- **프레임버퍼 없음**: TFT 직접 Push 방식으로 115KB 절약
- **JPEG 스트리밍**: JPEGDEC 콜백 방식, 파일 크기만큼만 임시 할당
- **dirty rect 렌더링**: 시계 숫자 변경 자리만 덮어쓰기

---

## 구현 현황

| 기능 | 상태 |
|------|------|
| WiFi 자동 연결 (WiFiManager) | ✅ 확인 |
| NTP 시각 동기 (KST) | ✅ 확인 |
| 기본 디지털 시계 (clock 모드) | ✅ 확인 |
| OTA 펌웨어 업데이트 (`/update`) | ✅ 확인 |
| 드로잉 커맨드 (`POST /draw`) | ✅ 확인 |
| timeout 자동 시계 복귀 | ✅ 확인 |
| 장치 상태 API (`GET /`) | ✅ 확인 |
| JPEG 수신·표시 (`POST /display`) | ✅ 코드 완성 (미테스트) |
| WebSocket 스트리밍 | ⬜ Phase 3 예정 |

---

## Pi 클라이언트 (`pi/draw_client.py`)

| 함수/클래스 | 역할 |
|-------------|------|
| `SmallTV(host)` | 클라이언트 객체 |
| `tv.draw(elements, timeout=0)` | 드로잉 커맨드 전송 |
| `tv.clock()` | 시계 모드로 복귀 |
| `tv.clear(bg)` | 화면 지우기 |
| `tv.status()` | 장치 상태 JSON 반환 |
| `tv.brightness(level)` | 백라이트 조절 |
| `text(x_or_align, y, content, …)` | 텍스트 엘리먼트 생성 헬퍼 |
| `rect(x, y, w, h, …)` | 사각형 엘리먼트 생성 헬퍼 |
| `circle(x, y, r, …)` | 원 엘리먼트 생성 헬퍼 |
| `line(x1,y1,x2,y2,…)` | 직선 엘리먼트 생성 헬퍼 |
| `hline(x,y,length,…)` | 수평선 엘리먼트 생성 헬퍼 |
| `vline(x,y,length,…)` | 수직선 엘리먼트 생성 헬퍼 |
