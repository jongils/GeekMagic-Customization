# GeekMagic SmallTV Ultra — 커스텀 ESP8266 펌웨어

GeekMagic SmallTV Ultra의 공식 펌웨어를 **완전히 대체**하는 오픈소스 커스텀 펌웨어입니다.  
ESP8266이 직접 날씨·시계를 렌더링하고, 기존 `push_client.py`와 100% 호환되는 HTTP API를 제공합니다.

> 개발 환경 구축은 **[DEVELOPMENT.md](DEVELOPMENT.md)** · 구현 현황·코드 구조는 **[IMPLEMENTATION.md](IMPLEMENTATION.md)** 를 참고하세요.

---

## 하드웨어 스펙 (역분석 확정)

| 항목 | 내용 |
|------|------|
| MCU | ESP8266MOD (ESP-12F) @ 160MHz |
| 디스플레이 | KAHD154010C10-V3 — 1.54" IPS 240×240 |
| 디스플레이 칩 | **ST7789V** |
| 플래시 | 4MB |
| 인터페이스 | Hardware SPI |

### 확정 핀 배치

| 신호 | GPIO | 비고 |
|------|------|------|
| MOSI | 13 | 하드웨어 SPI 고정 |
| SCLK | 14 | 하드웨어 SPI 고정 |
| CS | 15 | 디스플레이 측 미연결, ESP 부팅 스트랩용 |
| **DC** | **0** | 명령/데이터 구분선 |
| **RST** | **2** | 디스플레이 리셋 |
| **BL** | **5** | 백라이트 (PWM) |

> **주의:** DC=GPIO0, RST=GPIO2 조합은 직관과 반대입니다. GPIO0·GPIO2는 부팅 스트랩 핀이지만, 부팅 완료 후 TFT 제어에 사용 가능합니다.

---

## 기능

| 테마 번호 | 내용 |
|-----------|------|
| Theme 1 | 날씨 시계 (아이콘 + 기온 + 시각) |
| Theme 2 | 날씨 예보 (기온·체감·습도·풍속) |
| Theme 3 | 포토 앨범 — push_client.py JPEG 푸시 수신 |
| Theme 4 | 디지털 시계 (대형 폰트 + 날짜) |
| Theme 5 | 두 줄 디지털 시계 |
| Theme 6 | 시·분·초 분리 패널 시계 |
| Theme 7 | 심플 날씨 + 시계 |

### HTTP API (기존 firmware 100% 호환)

| 엔드포인트 | 설명 |
|------------|------|
| `GET /v.json` | 펌웨어 버전 정보 |
| `GET /app.json` | 현재 테마 번호 |
| `GET /set?theme=N` | 테마 전환 (1~7) |
| `GET /set?brt=N` | 백라이트 밝기 |
| `POST /doUpload` | JPEG 이미지 업로드 (multipart) |
| `GET /delete?f=PATH` | 파일 삭제 |
| `POST /config` | API Key · 도시 설정 (JSON) |
| `GET /update` | OTA 펌웨어 업데이트 |

---

## 개발 환경

- **빌드**: PlatformIO (VSCode 또는 CLI)
- **플래시 호스트**: Raspberry Pi 5
- **시리얼**: `/dev/serial0` (ttyAMA0), 115200 baud

### Pi ↔ ESP8266 플래시 연결

| Pi 5 핀 | ESP8266 |
|---------|---------|
| GPIO14 (TXD) | RX |
| GPIO15 (RXD) | TX |
| GND | GND |
| GPIO0 (플래시 진입용) | GPIO0 |

> 플래시 모드: GPIO0→GND 연결 후 전원 재투입 → 업로드 실행 → GPIO0 분리 후 재부팅

### 빌드 및 플래시

```bash
cd firmware

# 빌드
pio run -e esp8266

# 플래시 (GPIO0 GND 연결 + 전원 재투입 후 실행)
pio run -e esp8266 -t upload --upload-port /dev/serial0

# 파일시스템 업로드
pio run -e esp8266 -t uploadfs --upload-port /dev/serial0

# OTA (WiFi 연결 후)
pio run -e esp8266_ota
```

---

## 첫 실행 — WiFi 설정

1. 전원 투입 시 저장된 WiFi 없으면 AP 모드 자동 진입
2. 스마트폰/PC에서 **`GeekMagic-Setup`** WiFi 연결
3. 브라우저에서 **`192.168.4.1`** 접속
4. SSID / 비밀번호 입력 후 저장
5. 재부팅 후 자동 연결

---

## 날씨 API 설정

```bash
curl -X POST http://<장치IP>/config \
  -H "Content-Type: application/json" \
  -d '{"apiKey":"YOUR_OWM_API_KEY","city":"Seoul"}'
```

- [OpenWeatherMap](https://openweathermap.org/api) 무료 API Key 발급 필요
- 10분 주기 자동 갱신

---

## 소스 구조

```
firmware/
├── platformio.ini          ← 빌드 설정 (핀 배치, 드라이버)
├── src/
│   ├── main.cpp            ← 진입점 (setup / loop)
│   ├── config.h            ← 상수 정의 (핀, 테마, NTP 등)
│   ├── display.cpp/.h      ← TFT_eSPI 래퍼
│   ├── http_server.cpp/.h  ← HTTP API 서버
│   ├── filesystem.cpp/.h   ← LittleFS (이미지 저장/삭제)
│   ├── jpeg_display.cpp/.h ← JPEGDEC → TFT 스트리밍
│   ├── clock_theme.cpp/.h  ← Theme 4·5·6: 실시간 시계
│   └── weather_theme.cpp/.h← Theme 1·2·7: 날씨+시계
└── diag/
    └── main.cpp            ← 핀 탐색 진단 유틸리티
```

---

## 메모리 사용량

| 항목 | 사용량 |
|------|--------|
| Flash | ~59% / 1MB |
| RAM (정적) | ~75% / 80KB |
| 런타임 힙 여유 | ~18KB |

- 프레임버퍼 없음 (115KB 절약) — JPEGDEC 스트리밍 방식
- HTTPS 요청 시 BearSSL 세션을 스코프 종료 즉시 해제

---

## 핀 역분석 과정

공식 소스코드 없이 다음 방법으로 핀을 확정했습니다.

1. **플래시 덤프** — `esptool.py read_flash`로 원본 펌웨어 4MB 백업
2. **바이너리 분석** — 드라이버 문자열 탐색 (초기 GC9A01 오진단)
3. **부품 라벨 확인** — 디스플레이 모듈에 인쇄된 `KAHD154010C10-V3`
4. **커뮤니티 자료** — ESPHome GeekMagic Ultra 리버스 엔지니어링 결과 대조
5. **진단 펌웨어** — Raw SPI로 DC·RST·BL 핀 조합을 런타임에 순환 테스트하여 최종 확인

> DC=GPIO0, RST=GPIO2, BL=GPIO5 — 표준과 반대 배치이므로 주의

---

## 원본 펌웨어 복원

```bash
python3 ~/tools/esptool-venv/bin/esptool.py \
  --port /dev/serial0 --baud 460800 --no-stub \
  write_flash --flash_mode dio --flash_size 4MB \
  0x0 ~/tools/original_firmware.bin
```
