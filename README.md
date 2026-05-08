# GeekMagic SmallTV Ultra — Weather Clock & System Monitor

> Raspberry Pi 5와 GeekMagic SmallTV Ultra를 연동하는 날씨 시계 + 시스템 모니터 프로젝트  
> A weather clock and system monitor for GeekMagic SmallTV Ultra, powered by Raspberry Pi 5

---

## 한국어

### 개요

Raspberry Pi 5에서 날씨 정보와 시계를 담은 240×240 이미지를 생성하여 GeekMagic SmallTV Ultra 장치에 실시간으로 푸시하는 앱입니다. 별도의 펌웨어 수정 없이 장치 내장 HTTP API만을 사용합니다.

### 화면 구성

| 모드 | 설명 |
|------|------|
| GeekMagic 내장 테마 | 장치 자체 날씨/시계 화면 (theme 1~7 선택 가능) |
| CPU 모니터 | CPU 사용률, 메모리, 디스크, CPU 온도, 업타임 |

### 요구사항

- Raspberry Pi (Python 3.10 이상)
- GeekMagic SmallTV Ultra (펌웨어 Ultra-V9.0.x)
- 같은 로컬 네트워크 환경
- OpenWeatherMap API Key (무료 플랜, 없으면 더미 데이터로 동작)

### 설치

```bash
# 1. 저장소 클론
git clone https://github.com/jongils/GeekMagic-WeatherClock.git
cd GeekMagic-WeatherClock

# 2. 가상환경 생성 및 패키지 설치
python3 -m venv venv
venv/bin/pip install -r requirements.txt

# 3. 설정 파일 편집
nano config.json

# 4. (선택) systemd 서비스 등록 — 부팅 시 자동 시작
bash install.sh
```

### 설정 (`config.json`)

```json
{
  "device_ip": "192.168.x.x",
  "city": "Seoul",
  "api_key": "YOUR_OPENWEATHERMAP_API_KEY",
  "temp_unit": "metric",
  "time_format": "24h",
  "refresh_interval_sec": 60,
  "weather_interval_min": 10,
  "web_port": 8080,
  "night_mode": {
    "enabled": false,
    "start": "23:00",
    "end": "07:00"
  },
  "cpu_monitor": {
    "enabled": false,
    "show_sec": 10,
    "rest_sec": 50,
    "restore_theme": 1
  }
}
```

| 항목 | 설명 | 기본값 |
|------|------|--------|
| `device_ip` | GeekMagic 장치 IP 주소 | — |
| `city` | 날씨 조회 도시명 (OpenWeatherMap 기준) | `Pyeongtaek-si` |
| `api_key` | OpenWeatherMap API Key | `""` (없으면 더미 데이터) |
| `temp_unit` | 온도 단위 `metric`(°C) / `imperial`(°F) | `metric` |
| `time_format` | 시간 형식 `24h` / `12h` | `24h` |
| `refresh_interval_sec` | 날씨 화면 업데이트 주기 (초) | `60` |
| `weather_interval_min` | 날씨 API 호출 주기 (분) | `10` |
| `web_port` | 웹 설정 UI 포트 | `8080` |
| `night_mode.enabled` | 야간 모드 (지정 시간대 Push 중단) | `false` |
| `cpu_monitor.enabled` | CPU 모니터 교대 표시 활성화 | `false` |
| `cpu_monitor.show_sec` | CPU 정보 표시 시간 (초) | `10` |
| `cpu_monitor.rest_sec` | GeekMagic 내장 테마 표시 시간 (초) | `50` |
| `cpu_monitor.restore_theme` | CPU 종료 후 복원할 내장 테마 번호 (0=날씨 커스텀) | `1` |

### 실행

**메인 서비스**
```bash
venv/bin/python3 main.py
```
웹 설정 UI: `http://[라즈베리파이 IP]:8080`

CPU 모니터 교대 표시는 웹 UI 또는 `config.json`의 `cpu_monitor.enabled`로 켜고 끌 수 있습니다.  
설정 변경은 서비스 재시작 없이 다음 사이클부터 자동 반영됩니다.

> **주의**: `main.py`가 실행 중일 때 `cpu_monitor.py`를 별도로 실행하면 두 프로세스가 장치 테마를 서로 덮어써 충돌이 발생합니다. CPU 모니터는 반드시 `main.py` 내 통합 스케줄러를 사용하세요.

**CPU 모니터 standalone (main.py 미실행 시 전용)**
```bash
venv/bin/python3 cpu_monitor.py
```
CPU 정보 표시 → 장치 내장 테마 복원 → 반복 / 종료: `Ctrl+C`

### 프로젝트 구조

```
├── main.py                  # 메인 진입점 (날씨+시계 + CPU 모니터 통합)
├── cpu_monitor.py           # CPU 모니터 standalone (main.py 미실행 시 전용)
├── config.json              # 설정 파일
├── requirements.txt
├── install.sh               # systemd 서비스 등록 스크립트
├── src/
│   ├── weather_api.py       # OpenWeatherMap API 연동
│   ├── image_generator.py   # Pillow 날씨+시계 이미지 생성
│   ├── cpu_image.py         # Pillow CPU 모니터 이미지 생성 (공용 모듈)
│   ├── push_client.py       # GeekMagic HTTP API 클라이언트
│   ├── scheduler.py         # 스케줄러 (날씨+CPU 스레드 조율)
│   └── web_config.py        # Flask 웹 설정 서버
├── templates/
│   └── index.html           # 웹 설정 UI
└── cache/                   # 날씨 JSON 캐시 + 이미지 캐시
```

### 아키텍처

```
main.py
  ├── [scheduler thread]    날씨+시계 이미지 → Push (매 N초)
  │     └─ 내장 테마 모드 또는 CPU 표시 중이면 자동 skip
  ├── [cpu-monitor thread]  rest_sec 대기 → CPU 이미지 Push → show_sec 표시 → 테마 복원 → 반복
  │     └─ threading.Lock으로 날씨 Push와 충돌 방지
  └── [Flask thread]        웹 설정 UI (포트 8080)
```

### 장치 HTTP API

| 엔드포인트 | 설명 |
|-----------|------|
| `POST /doUpload?dir=/image/` | 이미지 업로드 (multipart/form-data) |
| `GET /set?theme=N` | 테마 전환 (3=포토앨범, 1·2·4~7=내장 테마) |
| `GET /set?img=/image//파일명` | 표시 이미지 지정 (theme=3 상태에서만 유효) |
| `GET /app.json` | 현재 테마 조회 |
| `GET /img.json` | 현재 표시 이미지 경로 조회 |
| `GET /v.json` | 펌웨어 버전 조회 |

> **주의 1**: `requests` 라이브러리는 ESP8266의 `Content-Length` 중복 헤더 버그로 사용 불가. `http.client`를 직접 사용합니다.  
> **주의 2**: `/set?theme=3&img=...` 조합 쿼리는 ESP8266 펌웨어가 `img=`를 무시합니다. 테마 전환과 이미지 지정은 반드시 별도 요청으로 분리해야 합니다.

---

## English

### Overview

This project pushes real-time weather and clock information (or CPU stats) to a GeekMagic SmallTV Ultra display (240×240) from a Raspberry Pi 5. It uses only the device's built-in HTTP API — no firmware modification required.

### Display Modes

| Mode | Content |
|------|---------|
| GeekMagic Built-in Theme | Device's own weather/clock display (selectable theme 1–7) |
| CPU Monitor | CPU usage, memory, disk, temperature, uptime |

### Requirements

- Raspberry Pi (Python 3.10+)
- GeekMagic SmallTV Ultra (firmware Ultra-V9.0.x)
- Same local network
- OpenWeatherMap API Key (free tier; falls back to dummy data if not provided)

### Installation

```bash
# 1. Clone the repository
git clone https://github.com/jongils/GeekMagic-WeatherClock.git
cd GeekMagic-WeatherClock

# 2. Create virtual environment and install dependencies
python3 -m venv venv
venv/bin/pip install -r requirements.txt

# 3. Edit configuration
nano config.json

# 4. (Optional) Register as systemd service for auto-start on boot
bash install.sh
```

### Configuration (`config.json`)

```json
{
  "device_ip": "192.168.x.x",
  "city": "Seoul",
  "api_key": "YOUR_OPENWEATHERMAP_API_KEY",
  "temp_unit": "metric",
  "time_format": "24h",
  "refresh_interval_sec": 60,
  "weather_interval_min": 10,
  "web_port": 8080,
  "night_mode": {
    "enabled": false,
    "start": "23:00",
    "end": "07:00"
  },
  "cpu_monitor": {
    "enabled": false,
    "show_sec": 10,
    "rest_sec": 50,
    "restore_theme": 1
  }
}
```

| Key | Description | Default |
|-----|-------------|---------|
| `device_ip` | GeekMagic device IP address | — |
| `city` | City name for weather (OpenWeatherMap format) | `Pyeongtaek-si` |
| `api_key` | OpenWeatherMap API Key | `""` (uses dummy data if empty) |
| `temp_unit` | Temperature unit: `metric` (°C) or `imperial` (°F) | `metric` |
| `time_format` | Clock format: `24h` or `12h` | `24h` |
| `refresh_interval_sec` | Display update interval (seconds) | `60` |
| `weather_interval_min` | Weather API fetch interval (minutes) | `10` |
| `web_port` | Web config UI port | `8080` |
| `night_mode.enabled` | Suppress pushes during specified hours | `false` |
| `cpu_monitor.enabled` | Enable alternating CPU monitor display | `false` |
| `cpu_monitor.show_sec` | Duration to show CPU screen (seconds) | `10` |
| `cpu_monitor.rest_sec` | Duration to show GeekMagic built-in theme (seconds) | `50` |
| `cpu_monitor.restore_theme` | Built-in theme number to restore after CPU (0 = custom weather) | `1` |

### Usage

**Main service**
```bash
venv/bin/python3 main.py
```
Web config UI: `http://[raspberry-pi-ip]:8080`

Enable CPU monitor alternation via the web UI toggle or `config.json`.  
Config changes apply from the next cycle without restarting the service.

> **Warning**: Running `cpu_monitor.py` while `main.py` is active causes a race condition — both processes fight over the device theme. Use the integrated scheduler in `main.py` instead.

**CPU Monitor standalone (only when main.py is NOT running)**
```bash
venv/bin/python3 cpu_monitor.py
```
Cycles: CPU stats → built-in theme → repeat / Stop with `Ctrl+C`

### Project Structure

```
├── main.py                  # Entry point (weather clock + CPU monitor integrated)
├── cpu_monitor.py           # Standalone CPU monitor (use only without main.py)
├── config.json              # Configuration file
├── requirements.txt
├── install.sh               # systemd service registration script
├── src/
│   ├── weather_api.py       # OpenWeatherMap API integration
│   ├── image_generator.py   # Pillow weather+clock image renderer
│   ├── cpu_image.py         # Pillow CPU monitor image renderer (shared module)
│   ├── push_client.py       # GeekMagic HTTP API client
│   ├── scheduler.py         # Scheduler (weather+CPU thread coordination)
│   └── web_config.py        # Flask web config server
├── templates/
│   └── index.html           # Web config UI
└── cache/                   # Weather JSON cache + current image
```

### Architecture

```
main.py
  ├── [scheduler thread]    Weather image → Push every N seconds
  │     └─ Skipped when in built-in theme mode or CPU is displaying
  ├── [cpu-monitor thread]  Wait rest_sec → Push CPU image → Wait show_sec → Restore theme → repeat
  │     └─ threading.Lock prevents concurrent push with weather thread
  └── [Flask thread]        Web config UI (port 8080)
```

### Key Device API Endpoints

| Endpoint | Description |
|----------|-------------|
| `POST /doUpload?dir=/image/` | Upload image (multipart/form-data) |
| `GET /set?theme=N` | Switch theme (3=photo album, 1·2·4–7=built-in) |
| `GET /set?img=/image//filename` | Set display image (only valid in theme=3) |
| `GET /app.json` | Query current theme |
| `GET /img.json` | Query current display image path |
| `GET /v.json` | Query firmware version |

> **Note 1**: The `requests` library cannot be used due to an ESP8266 firmware bug that sends duplicate `Content-Length` headers. This project uses `http.client` directly.  
> **Note 2**: `/set?theme=3&img=...` combined query — the ESP8266 firmware ignores the `img=` parameter. Always send theme switch and image selection as **separate requests**.

### Known Limitations

- External images can only be displayed in Photo Album mode (`theme=3`)
- The device always references `weather_clock.jpg` internally — the filename cannot be changed
- `img.json` periodically resets to the internal saved path; the file content itself must be overwritten

### License

MIT

---

*Last updated: 2026-05-09*
