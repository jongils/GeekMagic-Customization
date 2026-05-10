# GeekMagic SmallTV Ultra — Weather Clock & System Monitor

> Raspberry Pi 5와 GeekMagic SmallTV Ultra를 연동하는 날씨 시계 + 시스템 모니터 프로젝트  
> A weather clock and system monitor for GeekMagic SmallTV Ultra, powered by Raspberry Pi 5

---

## 한국어

### 개요

Raspberry Pi 5에서 날씨 정보와 시계를 담은 240×240 이미지를 생성하여 GeekMagic SmallTV Ultra 장치에 실시간으로 푸시하는 앱입니다. 별도의 펌웨어 수정 없이 장치 내장 HTTP API만을 사용합니다.

### 화면 모드

| 모드 | 설명 |
|------|------|
| 날씨+시계 (커스텀) | Pillow로 생성한 날씨·시계 이미지를 장치에 Push |
| GeekMagic 내장 테마 | 장치 자체 날씨/시계 화면 (theme 1~7 선택 가능) |
| CPU 모니터 | CPU 사용률, 메모리, 디스크, CPU 온도, 업타임 표시 |
| 슬라이드쇼 | 로컬 폴더의 사진을 순서대로 반복 표시 |
| 콘솔 출력 | 셸 명령어 실행 결과를 실시간으로 표시 |

### 표시 우선순위

```
슬라이드쇼 > CPU 모니터 ≈ 콘솔 출력 > 날씨+시계
```

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
  },
  "slideshow": {
    "enabled": false,
    "folder": "/home/pi5/Pictures",
    "show_sec": 10,
    "rest_sec": 0,
    "shuffle": false,
    "restore_theme": 1
  },
  "console": {
    "enabled": false,
    "command": "vcgencmd measure_temp && free -h && df -h /",
    "label": "",
    "refresh_sec": 5,
    "show_sec": 30,
    "rest_sec": 0,
    "restore_theme": 1
  }
}
```

#### 기본 설정

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

#### CPU 모니터 (`cpu_monitor`)

| 항목 | 설명 | 기본값 |
|------|------|--------|
| `enabled` | CPU 모니터 교대 표시 활성화 | `false` |
| `show_sec` | CPU 정보 표시 시간 (초) | `10` |
| `rest_sec` | GeekMagic 내장 테마 표시 시간 (초) | `50` |
| `restore_theme` | CPU 종료 후 복원할 내장 테마 번호 (0=날씨 커스텀) | `1` |

#### 슬라이드쇼 (`slideshow`)

| 항목 | 설명 | 기본값 |
|------|------|--------|
| `enabled` | 슬라이드쇼 활성화 | `false` |
| `folder` | 사진 폴더 경로 | `/home/pi5/Pictures` |
| `show_sec` | 사진 1장 표시 시간 (초) | `10` |
| `rest_sec` | 사진 사이 내장 테마 표시 시간 (0=연속 재생) | `0` |
| `shuffle` | 랜덤 순서 재생 | `false` |
| `restore_theme` | 사진 사이 복원할 내장 테마 번호 | `1` |

#### 콘솔 출력 (`console`)

| 항목 | 설명 | 기본값 |
|------|------|--------|
| `enabled` | 콘솔 출력 표시 활성화 | `false` |
| `command` | 실행할 셸 명령어 | Pi 상태 조회 명령 |
| `label` | 타이틀 바 텍스트 (비워두면 명령어 앞부분 표시) | `""` |
| `refresh_sec` | 연속 모드: 명령 재실행 주기 (초) | `5` |
| `show_sec` | 교대 모드: 콘솔 표시 시간 (초) | `30` |
| `rest_sec` | 0=연속 표시, N=교대 표시 (내장 테마 표시 시간) | `0` |
| `restore_theme` | 교대 모드 종료 후 복원할 내장 테마 번호 | `1` |

### 실행

**메인 서비스**
```bash
venv/bin/python3 main.py
```
웹 설정 UI: `http://[라즈베리파이 IP]:8080`

모든 기능(슬라이드쇼, CPU 모니터, 콘솔 출력)은 웹 UI 또는 `config.json`으로 켜고 끌 수 있습니다.

| 동작 | 반영 시점 |
|------|-----------|
| 기능 **켜기** (토글 ON) | 설정 저장 즉시 스레드 시작 |
| 기능 **끄기** (토글 OFF) | 최대 1초 이내 스레드 종료 + 화면 자동 복원 |
| 숫자·문자열 설정 변경 | 다음 사이클 시작 시 자동 반영 (재시작 불필요) |

기능을 끄면 해당 스레드가 즉시 종료되고, `restore_theme`에 설정된 내장 테마(또는 날씨+시계 화면)로 장치 화면이 자동으로 복원됩니다.

> **주의 1**: `main.py`가 실행 중일 때 `cpu_monitor.py`를 별도로 실행하면 두 프로세스가 장치 테마를 서로 덮어써 충돌이 발생합니다. CPU 모니터는 반드시 `main.py` 내 통합 스케줄러를 사용하세요.

> **주의 2 (코드 변경 후 반드시 재시작)**: `config.json` 설정 값은 실시간 반영되지만, `src/*.py` 또는 `templates/index.html` 파일을 직접 수정한 경우에는 서비스를 재시작해야 변경 사항이 적용됩니다. Python 모듈은 서비스 기동 시 메모리에 로드되며 이후 파일이 바뀌어도 자동으로 다시 읽지 않습니다. (Flask 템플릿은 요청마다 디스크에서 읽으나, Python 코드는 재시작 필수)
> ```bash
> sudo systemctl restart weather-clock
> ```

**CPU 모니터 standalone (main.py 미실행 시 전용)**
```bash
venv/bin/python3 cpu_monitor.py
```
CPU 정보 표시 → 장치 내장 테마 복원 → 반복 / 종료: `Ctrl+C`

### 프로젝트 구조

```
├── main.py                  # 메인 진입점 (통합 스케줄러)
├── cpu_monitor.py           # CPU 모니터 standalone (main.py 미실행 시 전용)
├── config.json              # 설정 파일
├── requirements.txt
├── install.sh               # systemd 서비스 등록 스크립트
├── src/
│   ├── weather_api.py       # OpenWeatherMap API 연동
│   ├── image_generator.py   # Pillow 날씨+시계 이미지 생성
│   ├── cpu_image.py         # Pillow CPU 모니터 이미지 생성
│   ├── slideshow.py         # 슬라이드쇼 이미지 처리 (리사이즈·EXIF·캐시)
│   ├── console_image.py     # 명령어 출력 → 240×240 이미지 렌더링
│   ├── push_client.py       # GeekMagic HTTP API 클라이언트
│   ├── scheduler.py         # 멀티스레드 스케줄러 (표시 우선순위 조율)
│   └── web_config.py        # Flask 웹 설정 서버
├── templates/
│   └── index.html           # 웹 설정 UI
└── cache/
    ├── current.jpg          # 현재 날씨+시계 이미지
    ├── cpu_current.jpg      # 현재 CPU 모니터 이미지
    ├── slideshow/           # 슬라이드쇼 처리 이미지 캐시
    └── console/             # 콘솔 출력 이미지 캐시
```

### 아키텍처

```
main.py
  ├── [scheduler thread]    날씨+시계 이미지 → Push (매 N초)
  │     └─ 슬라이드쇼·콘솔·CPU 표시 중이면 자동 skip
  ├── [cpu-monitor thread]  내장 테마(rest_sec) → CPU Push(show_sec) → 테마 복원 → 반복
  ├── [slideshow thread]    사진 리사이즈 → Push(show_sec) → [테마(rest_sec)] → 반복
  ├── [console thread]      명령 실행 → 이미지 Push → 대기 → 반복
  │     연속 모드: refresh_sec마다 갱신 / 교대 모드: 내장 테마 → 콘솔 → 반복
  └── [Flask thread]        웹 설정 UI (포트 8080)

모든 Push는 threading.Lock(_push_lock)으로 동시 실행 방지
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
> **주의 2**: `/set?theme=3&img=...` 조합 쿼리는 ESP8266 펌웨어가 `img=`를 무시합니다. 이미지 업로드 → 테마 전환 → 이미지 지정을 반드시 **별도 3단계 요청**으로 분리해야 합니다.

### 웹 설정 UI 엔드포인트

| 경로 | 설명 |
|------|------|
| `GET /` | 설정 메인 페이지 |
| `POST /config` | 설정 저장 |
| `GET /status` | 장치 연결 상태 + 현재 시각 JSON |
| `GET /push-now` | 날씨+시계 화면 즉시 업데이트 |
| `GET /preview` | 현재 날씨+시계 이미지 미리보기 |
| `GET /slideshow-info?folder=` | 지정 폴더의 이미지 수 반환 |
| `GET /console-preview?command=&label=` | 명령어 실행 결과 이미지 즉시 생성 반환 |

---

## English

### Overview

This project pushes real-time weather, clock, CPU stats, slideshows, and console output as 240×240 images to a GeekMagic SmallTV Ultra display from a Raspberry Pi 5. It uses only the device's built-in HTTP API — no firmware modification required.

### Display Modes

| Mode | Content |
|------|---------|
| Weather + Clock (custom) | Pillow-rendered weather and clock image pushed to device |
| GeekMagic Built-in Theme | Device's own weather/clock display (selectable theme 1–7) |
| CPU Monitor | CPU usage, memory, disk, temperature, uptime |
| Slideshow | Cycles through photos from a local folder |
| Console Output | Renders shell command output as a live display |

### Display Priority

```
Slideshow > CPU Monitor ≈ Console > Weather + Clock
```

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

#### Core Settings

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

#### CPU Monitor (`cpu_monitor`)

| Key | Description | Default |
|-----|-------------|---------|
| `enabled` | Enable alternating CPU monitor display | `false` |
| `show_sec` | Duration to show CPU screen (seconds) | `10` |
| `rest_sec` | Duration to show GeekMagic built-in theme (seconds) | `50` |
| `restore_theme` | Built-in theme number to restore after CPU (0 = custom weather) | `1` |

#### Slideshow (`slideshow`)

| Key | Description | Default |
|-----|-------------|---------|
| `enabled` | Enable slideshow | `false` |
| `folder` | Path to photo folder | `/home/pi5/Pictures` |
| `show_sec` | Duration to show each photo (seconds) | `10` |
| `rest_sec` | Built-in theme display time between photos (0 = continuous) | `0` |
| `shuffle` | Randomize photo order | `false` |
| `restore_theme` | Built-in theme number to show between photos | `1` |

#### Console Output (`console`)

| Key | Description | Default |
|-----|-------------|---------|
| `enabled` | Enable console output display | `false` |
| `command` | Shell command to execute | Pi status command |
| `label` | Title bar text (omit to show command prefix) | `""` |
| `refresh_sec` | Continuous mode: re-run interval (seconds) | `5` |
| `show_sec` | Alternating mode: console display duration (seconds) | `30` |
| `rest_sec` | 0 = continuous display; N = alternating (theme duration in seconds) | `0` |
| `restore_theme` | Built-in theme number to restore after console (alternating mode) | `1` |

### Usage

**Main service**
```bash
venv/bin/python3 main.py
```
Web config UI: `http://[raspberry-pi-ip]:8080`

All features (slideshow, CPU monitor, console output) can be toggled via the web UI or `config.json`.

| Action | When it takes effect |
|--------|---------------------|
| **Enable** a feature (toggle ON) | Thread starts immediately on save |
| **Disable** a feature (toggle OFF) | Thread stops within 1 second + display auto-restored |
| Change numeric/string settings | Applied at the start of the next cycle (no restart needed) |

When a feature is disabled, its thread exits immediately and the device display is automatically restored to the configured `restore_theme` (built-in theme or custom weather screen).

> **Warning 1**: Running `cpu_monitor.py` while `main.py` is active causes a race condition — both processes fight over the device theme. Use the integrated scheduler in `main.py` instead.

> **Warning 2 (restart required after code changes)**: While `config.json` values are reloaded at runtime, any changes to `src/*.py` or `templates/index.html` require a service restart to take effect. Python modules are loaded into memory at startup and are not reloaded automatically when files change on disk. (Flask templates are re-read per request, but Python code is not.)
> ```bash
> sudo systemctl restart weather-clock
> ```

**CPU Monitor standalone (only when main.py is NOT running)**
```bash
venv/bin/python3 cpu_monitor.py
```
Cycles: CPU stats → built-in theme → repeat / Stop with `Ctrl+C`

### Project Structure

```
├── main.py                  # Entry point (integrated multi-thread scheduler)
├── cpu_monitor.py           # Standalone CPU monitor (use only without main.py)
├── config.json              # Configuration file
├── requirements.txt
├── install.sh               # systemd service registration script
├── src/
│   ├── weather_api.py       # OpenWeatherMap API integration
│   ├── image_generator.py   # Pillow weather+clock image renderer
│   ├── cpu_image.py         # Pillow CPU monitor image renderer
│   ├── slideshow.py         # Slideshow image processing (resize, EXIF, cache)
│   ├── console_image.py     # Shell command output → 240×240 image renderer
│   ├── push_client.py       # GeekMagic HTTP API client
│   ├── scheduler.py         # Multi-thread scheduler (display priority coordination)
│   └── web_config.py        # Flask web config server
├── templates/
│   └── index.html           # Web config UI
└── cache/
    ├── current.jpg          # Current weather+clock image
    ├── cpu_current.jpg      # Current CPU monitor image
    ├── slideshow/           # Processed slideshow image cache
    └── console/             # Console output image cache
```

### Architecture

```
main.py
  ├── [scheduler thread]    Weather image → Push every N seconds
  │     └─ Skipped when slideshow/console/CPU is displaying
  ├── [cpu-monitor thread]  Built-in theme(rest_sec) → CPU Push(show_sec) → restore → repeat
  ├── [slideshow thread]    Resize photo → Push(show_sec) → [theme(rest_sec)] → repeat
  ├── [console thread]      Run command → render image → Push → wait → repeat
  │     Continuous: refresh every N sec / Alternating: theme → console → repeat
  └── [Flask thread]        Web config UI (port 8080)

All pushes are serialized via threading.Lock (_push_lock)
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
> **Note 2**: `/set?theme=3&img=...` combined query — the ESP8266 firmware silently ignores the `img=` parameter. Always send as **three separate requests**: upload → `set?theme=3` → `set?img=`.

### Known Limitations

- External images can only be displayed in Photo Album mode (`theme=3`)
- The device always references `weather_clock.jpg` internally — the filename cannot be changed
- `img.json` periodically resets to the internal saved path; the file content itself must be overwritten

### License

MIT

---

*Last updated: 2026-05-10*
