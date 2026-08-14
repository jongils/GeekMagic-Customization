# 구현 현황 및 코드 구조

GeekMagic SmallTV Ultra 커스텀 펌웨어의 전체 구현 상태와 각 기능에 대응하는 코드를 정리합니다.

---

## 전체 아키텍처

```
┌─────────────────────────────────────────────────────────┐
│                    main.cpp (진입점)                     │
│  setup(): 디스플레이→파일시스템→WiFi→NTP→HTTP서버 초기화 │
│  loop():  HTTP처리 / NTP재동기 / 날씨갱신 / 1초클럭     │
└────────┬───────────────────────────────────────────────┘
         │ 의존
    ┌────▼────┐  ┌──────────┐  ┌─────────────┐  ┌──────────────┐
    │display  │  │filesystem│  │ http_server │  │ clock_theme  │
    │  .cpp   │  │  .cpp    │  │   .cpp      │  │   .cpp       │
    └────┬────┘  └────┬─────┘  └──────┬──────┘  └──────────────┘
         │            │               │         ┌──────────────┐
       TFT_eSPI   LittleFS      ESP8266          │weather_theme │
       ST7789V    (4MB Flash)   WebServer        │   .cpp       │
                                                 └──────────────┘
                                                 ┌──────────────┐
                                                 │jpeg_display  │
                                                 │   .cpp       │
                                                 └──────────────┘
```

---

## 모듈별 구현 상세

### 1. `config.h` — 전역 상수 정의

모든 핀 번호, 테마 번호, NTP 설정, 경로 상수를 한 곳에서 관리합니다.

```
핵심 상수
├── 하드웨어 핀: TFT_DC=0, TFT_RST=2, TFT_BL=5 (platformio.ini에서 -D 플래그로 주입)
├── 테마 번호: THEME_WEATHER_CLOCK=1 ~ THEME_SIMPLE_WEATHER=7
├── NTP: pool.ntp.org, KST UTC+9
├── 파일 경로: /image/weather_clock.jpg (업로드 이미지 고정 경로)
├── 디스플레이: DISPLAY_W=240, DISPLAY_H=240
└── DEBUG 플래그: Serial.printf 로그 ON/OFF
```

**구현 상태: 완료**

---

### 2. `display.cpp / display.h` — TFT 드라이버 래퍼

TFT_eSPI 라이브러리를 감싸서 상위 모듈이 하드웨어를 직접 다루지 않도록 추상화합니다.

| 함수 | 역할 |
|------|------|
| `displayInit()` | TFT 초기화, rotation=0, 백라이트 PWM 켜기 |
| `displayFill(colour)` | 전체 화면 단색 채우기 |
| `displayText(x,y,text,colour,size)` | 좌표 지정 텍스트 출력 |
| `displayTextCentre(x,y,w,text,…)` | 너비 w 범위 안에서 가운데 정렬 텍스트 |
| `displayTextRight(x,y,text,…)` | 우측 정렬 텍스트 |
| `displayHLine(x,y,len,colour)` | 수평선 그리기 |
| `displayRect(x,y,w,h,colour)` | 사각형 채우기 (부분 화면 지우기에 사용) |
| `displayRoundRect(…)` | 모서리 둥근 사각형 외곽선 |
| `displaySetBrightness(level)` | 백라이트 PWM (0~255) |
| `displayBootSplash()` | 부팅 시 로고 화면 표시 |

> **표시 영역 주의:** 물리 케이스 베젤로 인해 **y < 75 영역은 실제로 가려짐**.  
> 안전 표시 영역은 **y ≥ 75**부터 시작.

**구현 상태: 완료**

---

### 3. `filesystem.cpp / filesystem.h` — LittleFS 파일 시스템

이미지 저장, 설정 파일 보존에 사용합니다. ESP8266 플래시 4MB 중 파일시스템 파티션을 LittleFS로 포맷합니다.

| 함수 | 역할 |
|------|------|
| `fsInit()` | LittleFS 마운트 (실패 시 자동 포맷 후 재시도) |
| `fsSaveUpload(data, len)` | `/image/weather_clock.jpg`에 바이너리 저장 |
| `fsDelete(path)` | 파일 삭제 |
| `fsExists(path)` | 파일 존재 여부 확인 |
| `fsFileSize(path)` | 파일 크기 반환 |

LittleFS에 저장되는 파일들:

| 경로 | 내용 |
|------|------|
| `/image/weather_clock.jpg` | push_client.py가 푸시한 날씨 이미지 |
| `/state.json` | 현재 테마 번호·밝기 (재부팅 복원용) |
| `/weather.json` | OpenWeatherMap API Key·도시 이름 |

**구현 상태: 완료**

---

### 4. `http_server.cpp / http_server.h` — HTTP API 서버

기존 `push_client.py`와 완전히 호환되는 HTTP 엔드포인트를 제공합니다.  
포트 80, `ESP8266WebServer` 기반.

#### 엔드포인트 목록

| 메서드 | 경로 | 설명 | 구현 상태 |
|--------|------|------|-----------|
| GET | `/` | 헬스체크 ("GeekMagic Custom Firmware OK") | ✅ |
| GET | `/v.json` | 펌웨어 버전 `{"m":"SmallTV-Ultra","v":"Custom-V1.0.0"}` | ✅ |
| GET | `/app.json` | 현재 테마 번호 `{"theme":N}` | ✅ |
| GET | `/img.json` | 현재 이미지 경로 `{"img":"…"}` | ✅ |
| GET | `/brt.json` | 현재 밝기 `{"brt":"N"}` | ✅ |
| GET | `/set?theme=N` | 테마 전환 (1~7), 즉시 화면 반영 | ✅ |
| GET | `/set?brt=N` | 백라이트 밝기 조절 (0~255) | ✅ |
| GET | `/set?img=PATH` | 이미지 경로 지정 (Theme 3용) | ✅ |
| GET | `/delete?f=PATH` | LittleFS 파일 삭제 | ✅ |
| POST | `/doUpload` | JPEG 이미지 업로드 (multipart/form-data, field: `file`) | ✅ |
| POST | `/config` | API Key·도시 설정 `{"apiKey":"…","city":"…"}` | ✅ |
| GET/POST | `/update` | OTA 펌웨어 업데이트 (HTML 폼 + 바이너리 수신) | ✅ |

#### 상태 영속성

테마 번호와 밝기는 `/state.json`에 저장되어 전원을 껐다 켜도 복원됩니다.

#### OTA 업데이트 방법

```bash
curl -F "image=@firmware.bin" http://<장치IP>/update
```

**구현 상태: 완료**

---

### 5. `jpeg_display.cpp / jpeg_display.h` — JPEG → TFT 스트리밍

JPEGDEC 라이브러리를 사용해 JPEG를 디코딩하고 TFT에 직접 출력합니다.  
전체 프레임버퍼(115KB)를 사용하지 않고, 디코딩된 블록을 콜백으로 즉시 `pushImage()` 합니다.

```
흐름: LittleFS → malloc(파일크기) → JPEGDEC.openRAM()
      → jpegDraw 콜백 반복 호출 → tft.pushImage() → free()
```

| 함수 | 역할 |
|------|------|
| `jpegDisplayFile(path)` | 경로의 JPEG를 읽어 TFT 전체에 표시 |

**구현 상태: 완료**

---

### 6. `clock_theme.cpp / clock_theme.h` — 시계 테마 (Theme 4·5·6)

NTP로 동기화된 시각을 1초마다 화면에 렌더링합니다.  
변경된 영역만 재렌더링하여 깜빡임을 최소화합니다.

#### Theme 4 — 대형 디지털 시계 (기본 테마)

```
┌──────────────────────────────┐ ← y=0  (베젤에 가려짐)
│                              │
│    ██:██  (HH:MM, size=4)   │ ← y=76  (CYAN)
│                              │
│     :SS   (size=2)           │ ← y=152 (GREY)
│ ─────────────────────────── │ ← y=183 구분선
│   2025.08.15 FRI  (size=1)  │ ← y=190 (WHITE)
└──────────────────────────────┘ ← y=240
```

#### Theme 5 — 두 줄 스타일 시계

```
HH:MM  (size=4, YELLOW)  y=60
-- SS --  (size=2, GREY)  y=138
DAY YYYY/MM/DD  (size=1)  y=182
```

#### Theme 6 — 시·분·초 분리 패널

```
HOUR  (label)  y=10
██    (HH, size=4, CYAN)    y=22
MIN   (label)  y=90
██    (MM, size=4, YELLOW)  y=102
SEC   (label)  y=170
██    (SS, size=4, WHITE)   y=182
```

| 함수 | 역할 |
|------|------|
| `clockThemeInit()` | 캐시 변수 초기화 (강제 전체 재렌더링 트리거) |
| `clockTimeValid()` | NTP 동기화 완료 여부 확인 |
| `clockThemeRender(theme)` | 현재 테마에 맞게 1초마다 호출되는 렌더링 |

> **TFT_eSPI 폰트 제한:** 내장 폰트는 ASCII만 지원합니다.  
> 한글 UTF-8 문자는 깨져서 출력되므로 요일은 영문(SUN~SAT)을 사용합니다.

**구현 상태: 완료 (테스트 통과)**

---

### 7. `weather_theme.cpp / weather_theme.h` — 날씨+시계 테마 (Theme 1·2·7)

OpenWeatherMap API를 10분마다 호출해 날씨 데이터를 캐시하고 표시합니다.

#### 날씨 데이터 구조

```cpp
struct WeatherData {
    char    description[48];  // 날씨 설명 (예: "clear sky")
    char    city[48];         // 도시 이름
    int16_t tempC;            // 현재 기온 (°C)
    int16_t feelsLike;        // 체감 기온
    uint8_t humidity;         // 습도 (%)
    uint8_t windSpeed;        // 풍속 (m/s)
    uint16_t weatherId;       // OWM 날씨 코드 (800=맑음 등)
    bool    valid;
};
```

#### HTTPS 힙 관리 전략

BearSSL 세션을 스코프 블록 안에서 생성하고, 블록 종료 시 즉시 해제합니다.  
요청당 약 20KB가 확보됩니다.

```cpp
{
    BearSSL::WiFiClientSecure client;
    client.setInsecure();   // 인증서 검증 생략 → ~10KB 절약
    // ... 요청 ...
} // 여기서 자동 해제
```

#### ArduinoJson 필터링

필요한 필드만 파싱해 힙 소비를 줄입니다.

```cpp
JsonDocument filter;
filter["name"] = filter["main"]["temp"] = filter["weather"][0]["id"] = … = true;
deserializeJson(doc, client, DeserializationOption::Filter(filter));
```

#### 날씨 아이콘 (벡터 그래픽, 폰트 독립)

TFT 기본 도형 API로 직접 그려서 외부 폰트/이미지 파일 의존성이 없습니다.

| OWM 코드 범위 | 아이콘 |
|--------------|--------|
| 800 | 태양 (원 + 8방향 광선) |
| 801~804 | 구름 (fillCircle 조합) |
| 500~599 | 비 (구름 + 빗방울 선) |
| 600~699 | 눈 (구름 + X자 결정) |
| 200~299 | 번개 (구름 + 삼각형 볼트) |

#### Theme 1 — 날씨+시계 통합

```
도시명 (size=2)               y=8
──────────────────────────────  y=34
HH:MM (size=4, CYAN)           y=40
날씨아이콘 (cx=60,cy=140)
기온 (size=3, ORANGE)          y=118
습도·풍속 (size=1)             y=158,174
──────────────────────────────  y=196
날씨설명 (size=1)              y=204
```

#### Theme 2 — 날씨 상세 정보

```
도시명 (size=2)     y=8
날씨아이콘 중앙     cy=90
현재기온 (size=1)   y=150
체감기온            y=167
습도                y=184
풍속                y=201
날씨설명            y=218
```

#### Theme 7 — 심플 날씨+시계

```
HH:MM (size=5, WHITE)    y=88
기온 (size=2, ORANGE)    y=160
날씨설명 (size=1)        y=186
```

| 함수 | 역할 |
|------|------|
| `weatherSetConfig(apiKey, city)` | API Key·도시 저장 (LittleFS /weather.json) |
| `weatherUpdate()` | 10분 간격으로 OWM API 호출 (비차단) |
| `weatherDataValid()` | 유효한 날씨 데이터 캐시 존재 여부 |
| `weatherThemeRender(theme)` | 테마별 렌더링 (1·2·7) |

**구현 상태: 코드 완성, API Key 미설정 시 동작 불가**

---

### 8. `main.cpp` — 진입점

#### setup() 초기화 순서

```
1. Serial 시작 (DEBUG=1일 때)
2. displayInit() → displayBootSplash()
3. fsInit() (LittleFS 마운트)
4. wifiSetup() — WiFiManager로 자동 연결
   ├── 저장된 SSID 있으면 즉시 연결
   └── 없으면 AP 모드 "GeekMagic-Setup" 진입, 브라우저 설정
5. NTP 동기화 (pool.ntp.org, KST UTC+9)
6. httpServerInit() (포트 80)
7. Ticker 1초 인터럽트 등록
8. clockThemeInit()
9. 마지막 테마 복원 (state.json 기반)
```

#### loop() 처리 순서

```
1. httpServerHandle() — 들어온 HTTP 요청 처리
2. ESP.wdtFeed()      — 워치독 타이머 리셋
3. syncPosixTime()    — NTP 재동기 (60초 간격, NTPClient 내부 관리)
4. weatherUpdate()    — 날씨 테마일 때만, 10분 간격 OWM 호출
5. if (_clockTick)    — 1초 인터럽트 플래그 확인
   └── clockThemeRender() 또는 weatherThemeRender() 호출
```

---

## 메모리 사용량 (현재 빌드 기준)

| 항목 | 사용 | 한도 |
|------|------|------|
| Flash | ~59% (620KB) | 1,044KB |
| RAM (정적) | ~75% (61KB) | 81KB |
| 런타임 힙 여유 | ~18KB | — |

### 메모리 절약 전략

- **프레임버퍼 없음**: TFT 직접 Push 방식으로 115KB 절약
- **BearSSL 스코프 해제**: HTTPS 요청 후 즉시 ~20KB 반환
- **ArduinoJson 필터**: 필요 필드만 파싱
- **dirty rect 렌더링**: 변경된 숫자 자리만 덮어쓰기

---

## 테마 전환 방법

```bash
# 테마 4: 디지털 시계 (기본)
curl "http://192.168.219.122/set?theme=4"

# 테마 3: 포토 앨범 (push_client.py 호환)
curl "http://192.168.219.122/set?theme=3"

# 날씨 API 설정 후 테마 1: 날씨+시계
curl -X POST http://192.168.219.122/config \
  -H "Content-Type: application/json" \
  -d '{"apiKey":"YOUR_KEY","city":"Seoul"}'
curl "http://192.168.219.122/set?theme=1"
```

---

## 확인된 동작 / 미확인 항목

| 기능 | 상태 |
|------|------|
| WiFi 자동 연결 | ✅ 확인 |
| NTP 시각 동기 (KST) | ✅ 확인 |
| Theme 4 디지털 시계 | ✅ 확인 |
| HTTP API 전체 엔드포인트 | ✅ 코드 완성 |
| OTA 펌웨어 업데이트 (`/update`) | ✅ 확인 |
| LittleFS 상태 영속 (`state.json`) | ✅ 코드 완성 |
| Theme 5·6 시계 | ⬜ 미확인 (코드 완성) |
| Theme 3 포토 앨범 (push_client.py) | ⬜ 미확인 (코드 완성) |
| Theme 1·2·7 날씨+시계 | ⬜ API Key 필요 |
| 날씨 아이콘 렌더링 | ⬜ 미확인 (코드 완성) |

---

## 다음 작업 제안

1. **날씨 테마 테스트**: OpenWeatherMap 무료 API Key 발급 → `/config` 설정 → Theme 1·2·7 확인
2. **push_client.py 호환성 테스트**: Theme 3 진입 후 기존 Python 스크립트로 이미지 푸시
3. **Theme 5·6 레이아웃 검증**: Theme 4에서 확인한 베젤 오프셋(y≥75) 적용 여부 점검
