# GeekMagic SmallTV Ultra — 커스텀 ESP8266 펌웨어

> 개발 환경 구축은 **[DEVELOPMENT.md](DEVELOPMENT.md)** · 코드 구조는 **[IMPLEMENTATION.md](IMPLEMENTATION.md)** 참고

---

## 하드웨어 스펙

| 항목 | 내용 |
|------|------|
| MCU | ESP8266MOD (ESP-12F) @ 160MHz |
| 디스플레이 | KAHD154010C10-V3 — 1.54" IPS 240×240 |
| 디스플레이 칩 | ST7789V |
| 플래시 | 4MB |
| 인터페이스 | Hardware SPI |

### 디스플레이 핀 배치 (TFT SPI)

| 신호 | GPIO | 비고 |
|------|------|------|
| MOSI | 13 | 하드웨어 SPI |
| SCLK | 14 | 하드웨어 SPI |
| CS | 15 | 디스플레이 측 미연결, ESP 부팅 스트랩용 |
| **DC** | **0** | 명령/데이터 구분 |
| **RST** | **2** | 디스플레이 리셋 |
| **BL** | **5** | 백라이트 PWM |

> GPIO0=DC, GPIO2=RST 는 직관과 반대 배치이므로 주의.

### 시리얼 프로그래밍 커넥터 (보드 6핀 패드)

| 핀 # | 신호 | 연결 대상 (Pi 기준) |
|------|------|---------------------|
| 1 | GND | Pi Pin 6 (GND) |
| 2 | TX | Pi Pin 10 (GPIO15 / RXD) |
| 3 | RX | Pi Pin 8 (GPIO14 / TXD) |
| 4 | VCC | 외부 3.3V 전원 |
| 5 | GPIO0 | GND와 단락 → 플래시 모드 |
| 6 | RST | 오픈 (선택) |

> 자세한 배선 및 핀맵 이미지 → **[DEVELOPMENT.md § 3](DEVELOPMENT.md#3-하드웨어-연결-pi--esp8266)**

---

## 동작 방식

ESP8266은 세 가지 디스플레이 모드를 가집니다.

| 모드 | 진입 방법 | 설명 |
|------|-----------|------|
| `clock` | 기본값 / `GET /mode?set=clock` | 1초 갱신 디지털 시계 |
| `draw` | `POST /draw` | Pi가 보낸 JSON 드로잉 커맨드 렌더링 |
| `jpeg` | `POST /display` or `/doUpload` | Pi가 보낸 JPEG 이미지 표시 |

`timeout` 파라미터를 지정하면 N초 후 자동으로 `clock` 모드로 복귀합니다.

---

## HTTP API

### 상태 / 제어

| 메서드 | 경로 | 설명 |
|--------|------|------|
| GET | `/` | 장치 상태 JSON `{"fw":…,"mode":…,"brt":…,"heap":…}` |
| GET | `/mode?set=clock` | 시계 모드로 복귀 |
| GET | `/set?brt=N` | 백라이트 밝기 (0–255) |
| GET/POST | `/update` | OTA 펌웨어 업데이트 |

### 드로잉 커맨드 (Phase 1 ✅)

`POST /draw` — Body: JSON

```json
{
  "clear":   true,
  "bg":      "black",
  "timeout": 30,
  "elements": [
    {"type": "text",   "y": 100, "text": "Hello", "size": 3, "color": "cyan", "align": "center"},
    {"type": "rect",   "x": 10,  "y": 10,  "w": 220, "h": 50, "color": "blue"},
    {"type": "line",   "x1": 0,  "y1": 75, "x2": 240, "y2": 75, "color": "grey"},
    {"type": "hline",  "x": 20,  "y": 190, "len": 200, "color": "white"},
    {"type": "vline",  "x": 120, "y": 80,  "len": 80,  "color": "white"},
    {"type": "circle", "x": 120, "y": 120, "r": 40, "color": "yellow", "fill": false}
  ]
}
```

**지원 엘리먼트**

| type | 주요 필드 | 비고 |
|------|-----------|------|
| `text` | x 또는 align, y, text, size(1-6), color | align: "left"·"center"·"right" |
| `rect` | x, y, w, h, color, fill(bool) | fill 기본 true |
| `circle` | x, y, r, color, fill(bool) | fill 기본 false |
| `line` | x1, y1, x2, y2, color | 직선 |
| `hline` | x, y, len, color | 수평선 |
| `vline` | x, y, len, color | 수직선 |

**색상**: `white` `black` `red` `green` `blue` `cyan` `yellow` `orange` `magenta` `grey` 또는 `"#RRGGBB"`

### JPEG 푸시 (Phase 2 ✅ 수신 가능)

`POST /display` 또는 `POST /doUpload` — multipart/form-data, field: `file`

### WebSocket (Phase 3 — 예정)

---

## Pi 클라이언트

```python
# pi/draw_client.py
from draw_client import SmallTV, text, hline, rect, circle

tv = SmallTV("192.168.219.122")

# 커스텀 화면 (10초 후 자동으로 시계 복귀)
tv.draw([
    text("center", 100, "Hello!", size=3, color="cyan"),
    hline(20, 130, 200, color="grey"),
    text("center", 145, "SmallTV Ultra", size=1, color="white"),
], timeout=10)

# 시계 모드로 즉시 복귀
tv.clock()

# 장치 상태 확인
print(tv.status())
```

---

## 빌드 및 업로드

### 시리얼 플래시

```bash
cd firmware

# 빌드
pio run -e esp8266

# 플래시 (GPIO0→GND 연결 후 전원 재투입)
pio run -e esp8266 -t upload --upload-port /dev/serial0
```

### OTA (WiFi 연결 후)

```bash
curl -F "image=@.pio/build/esp8266/firmware.bin" http://<장치IP>/update
```

---

## 첫 실행 — WiFi 설정

1. 전원 투입 시 저장된 WiFi 없으면 AP 모드 자동 진입
2. **`GeekMagic-Setup`** WiFi 연결
3. 브라우저에서 **`192.168.4.1`** 접속 → SSID / 비밀번호 입력
4. 재부팅 후 자동 연결 → 화면에 IP 주소 표시

---

## 표시 영역 주의사항

케이스 베젤로 인해 4면 모두 일부 영역이 가려집니다.  
`diag_bezel` 사각형 테스트로 측정한 실측 마진:

| 면 | 마진 | 안전 경계 |
|----|------|-----------|
| 상단 | 5px | y ≥ 5 |
| 하단 | 10px | y ≤ 229 |
| 좌측 | 5px | x ≥ 5 |
| 우측 | 5px | x ≤ 234 |

안전 표시 영역: **x 5~234, y 5~229** (230×225 px)

```
x=0   x=5              x=234 x=239
       │                │
y=0    ├────────────────┤
y=5    │  안전 표시 영역  │
       │   (230×225)    │
y=229  │                │
y=239  └────────────────┘
```

`config.h`의 `SAFE_X / SAFE_Y / SAFE_W / SAFE_H / SAFE_X2 / SAFE_Y2` 상수를 사용하세요.

---

## 메모리 사용량

| 항목 | 사용 | 한도 |
|------|------|------|
| Flash | ~48% (502KB) | 1,044KB |
| RAM (정적) | ~72% (59KB) | 81KB |
| 런타임 힙 | ~17KB | — |

---

## 시계 화면 구성 (Theme 4 기본)

```
y= 34: 날짜  "2026.08.17  MON"   size 2, 회색, 중앙
y= 56: ─────── 구분선 ───────
y= 70: HH:MM                    size 6 (48px), CYAN, 중앙
y=127: :SS                      size 3 (24px), 회색, 중앙
y=210: 🦀 게 아이콘 좌우 애니메이션
```

- 게 아이콘: `clock_theme.cpp`의 `iconDraw()` — TFT 프리미티브로 직접 그림
- 색상: `COL_CLAUDE = 0xDBCA` (Claude 코랄 ~RGB 222,121,82)
- 이동 범위: x 28 ~ 211, 왕복 10초 주기
- `clockAnimUpdate()` → `main.cpp`의 `loop()`에서 매 프레임 호출 (~60 fps)

---

## 소스 구조

```
firmware/
├── platformio.ini
├── src/
│   ├── main.cpp             ← 진입점 (setup/loop, 모드 기반 렌더링)
│   ├── config.h             ← 전역 상수 + BEZEL_* / SAFE_* 상수
│   ├── display.cpp/.h       ← TFT_eSPI 래퍼
│   ├── draw_cmd.cpp/.h      ← JSON 드로잉 커맨드 파서·렌더러
│   ├── http_server.cpp/.h   ← HTTP API 서버 (포트 80)
│   ├── filesystem.cpp/.h    ← LittleFS
│   ├── jpeg_display.cpp/.h  ← JPEG → TFT 스트리밍
│   └── clock_theme.cpp/.h   ← 시계 렌더링 + 🦀 애니메이션
├── diag/
│   └── main.cpp             ← 핀 탐색 진단 유틸리티
└── diag_bezel/
    └── main.cpp             ← 베젤 경계 테스트 (확정 경계선 시각화)

pi/
└── draw_client.py           ← Pi용 드로잉 커맨드 클라이언트
```

### 빌드 환경

| 환경 | 용도 |
|------|------|
| `esp8266` | 메인 펌웨어 빌드 + 시리얼 플래시 |
| `esp8266_ota` | 메인 펌웨어 OTA (실제로는 curl 사용) |
| `esp8266_diag` | 핀 진단 펌웨어 (`diag/`) |
| `esp8266_bezel` | 베젤 경계 테스트 펌웨어 (`diag_bezel/`) |
| `esp8266_bezel_ota` | 베젤 펌웨어 OTA 빌드 |

---

## 향후 작업 (미확인 / 미구현)

### ① ESP8266 GPIO 확장 가능성 조사

현재 디스플레이에 사용 중인 핀을 제외하고 실제로 사용 가능한 GPIO가 얼마나 남아 있는지 확인이 필요합니다.

| GPIO | 현재 용도 | 비고 |
|------|-----------|------|
| 0 | TFT DC | 부팅 스트랩 (LOW=플래시 모드) |
| 2 | TFT RST | 부팅 스트랩 (HIGH 필요) |
| 5 | TFT BL (PWM) | — |
| 13 | TFT MOSI | 하드웨어 SPI 고정 |
| 14 | TFT SCLK | 하드웨어 SPI 고정 |
| 15 | TFT CS | 부팅 스트랩 (LOW 필요) |
| 6–11 | 내부 SPI 플래시 | **사용 불가 (ESP-12F 내부 연결)** |
| 1 | UART TX | Serial 사용 시 점유 |
| 3 | UART RX | Serial 사용 시 점유 |
| **4** | **미사용** | I2C SDA 등 활용 가능 후보 |
| **12** | **미사용** | HSPI MISO, 범용 IO 후보 |
| **16** | **미사용** | Deep sleep 웨이크업 전용, 기능 제한 |

**확인 필요 사항**
- GPIO4·12·16의 PCB 실제 연결 상태 (회로도 추적 또는 멀티미터)
- 외부 센서(온습도, 조도 등) 또는 버튼 추가 가능 여부

---

### ② ESP8266 내부 SPI 플래시 핀맵 확인

ESP-12F 모듈은 내장 4MB SPI 플래시를 GPIO6–11에 연결해 사용합니다.  
이 핀들은 모듈 외부로 노출되지 않지만, 커스텀 PCB 설계나 플래시 교체 시 핀 충돌 방지를 위해 정확한 매핑 확인이 필요합니다.

| GPIO | SPI 플래시 신호 |
|------|----------------|
| 6 | CLK |
| 7 | MISO (Q) |
| 8 | MOSI (D) |
| 9 | HD (Write Protect) |
| 10 | WP |
| 11 | CS0 |

**확인 필요 사항**
- 현재 장착된 플래시 칩 부품 번호 (디캡 또는 데이터시트 대조)
- DIO / QIO 모드 여부 (`board_build.flash_mode = dio` 현재 설정)
- 외부 플래시 교체 시 용량 한계 (ESP8266 주소 공간 최대 16MB)

---

## 핀 역분석 과정

1. `esptool.py read_flash` 로 원본 펌웨어 덤프
2. 부품 라벨 `KAHD154010C10-V3` 확인 → ST7789V 확정
3. Raw SPI 진단 펌웨어로 DC·RST·BL 핀 조합 순환 테스트
4. DC=GPIO0, RST=GPIO2, BL=GPIO5 확정

### 원본 펌웨어 복원

```bash
python3 ~/tools/esptool-venv/bin/esptool.py \
  --port /dev/serial0 --baud 460800 --no-stub \
  write_flash --flash_mode dio --flash_size 4MB \
  0x0 ~/tools/original_firmware.bin
```
