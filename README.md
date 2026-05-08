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
| 날씨+시계 | 현재 시각, 날짜, 기온, 체감온도, 습도, 풍속, 구름량 |
| CPU 모니터 | CPU 사용률, 메모리, 디스크, CPU 온도, 업타임 (10초 주기 표시) |

### 요구사항

- Raspberry Pi (Python 3.10 이상)
- GeekMagic SmallTV Ultra (펌웨어 Ultra-V9.0.x)
- 같은 로컬 네트워크 환경
- OpenWeatherMap API Key (무료 플랜 사용 가능, 없으면 더미 데이터로 동작)

### 설치

```bash
# 1. 저장소 클론
git clone https://github.com/your-repo/geekmagic-weather-clock.git
cd geekmagic-weather-clock

# 2. 가상환경 생성 및 패키지 설치
python3 -m venv venv
venv/bin/pip install -r requirements.txt

# 3. 설정 파일 편집
nano config.json
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
  }
}
```

| 항목 | 설명 | 기본값 |
|------|------|--------|
| `device_ip` | GeekMagic 장치 IP 주소 | `192.168.219.119` |
| `city` | 날씨 조회 도시명 (OpenWeatherMap 기준) | `Pyeongtaek-si` |
| `api_key` | OpenWeatherMap API Key | `""` (없으면 더미 데이터) |
| `temp_unit` | 온도 단위 `metric`(°C) / `imperial`(°F) | `metric` |
| `time_format` | 시간 형식 `24h` / `12h` | `24h` |
| `refresh_interval_sec` | 화면 업데이트 주기 (초) | `60` |
| `weather_interval_min` | 날씨 API 호출 주기 (분) | `10` |
| `web_port` | 웹 설정 UI 포트 | `8080` |
| `night_mode` | 야간 모드 (지정 시간대 Push 중단) | 비활성화 |

### 실행

**날씨+시계 서비스 (메인)**
```bash
venv/bin/python3 main.py
```
웹 설정 UI: `http://[라즈베리파이 IP]:8080`

**CPU 모니터 (독립 실행)**
```bash
venv/bin/python3 cpu_monitor.py
```
CPU 정보 10초 표시 → 원래 테마(Weather Clock Today) 50초 → 반복  
종료: `Ctrl+C` (자동으로 원래 테마 복원)

**화면 표시 테스트**
```bash
venv/bin/python3 test_display.py
```

### 프로젝트 구조

```
├── main.py                  # 메인 진입점 (날씨+시계 + 웹 UI)
├── cpu_monitor.py           # CPU 모니터 (독립 실행)
├── test_display.py          # 화면 연결 테스트
├── config.json              # 설정 파일
├── requirements.txt
├── install.sh               # systemd 서비스 등록 스크립트
├── src/
│   ├── weather_api.py       # OpenWeatherMap API 연동
│   ├── image_generator.py   # Pillow 기반 240×240 이미지 생성
│   ├── push_client.py       # GeekMagic HTTP API 클라이언트
│   ├── scheduler.py         # 주기적 업데이트 스케줄러
│   └── web_config.py        # Flask 웹 설정 서버
├── templates/
│   └── index.html           # 웹 설정 UI
├── cache/                   # 날씨 JSON 캐시 + 현재 이미지
└── logs/                    # 실행 로그
```

### 장치 HTTP API 주요 엔드포인트

| 엔드포인트 | 설명 |
|-----------|------|
| `POST /doUpload?dir=/image/` | 이미지 업로드 (multipart/form-data) |
| `GET /set?theme=N` | 테마 전환 (1=날씨시계, 3=포토앨범) |
| `GET /set?theme=3&img=/image//파일명` | 테마+이미지 동시 지정 (깜박임 방지) |
| `GET /app.json` | 현재 테마 조회 |
| `GET /img.json` | 현재 표시 이미지 경로 조회 |
| `GET /v.json` | 펌웨어 버전 조회 |

> **주의**: `requests` 라이브러리는 ESP8266의 `Content-Length` 중복 헤더 버그로 사용 불가.  
> 이 프로젝트는 `http.client`를 직접 사용합니다.

---

## English

### Overview

This project pushes real-time weather and clock information to a GeekMagic SmallTV Ultra display (240×240) from a Raspberry Pi 5. It uses only the device's built-in HTTP API — no firmware modification required.

### Display Modes

| Mode | Content |
|------|---------|
| Weather Clock | Current time, date, temperature, feels-like, humidity, wind speed, cloud cover |
| CPU Monitor | CPU usage, memory, disk, CPU temperature, uptime (shown for 10 seconds periodically) |

### Requirements

- Raspberry Pi (Python 3.10+)
- GeekMagic SmallTV Ultra (firmware Ultra-V9.0.x)
- Same local network
- OpenWeatherMap API Key (free tier works; falls back to dummy data if not provided)

### Installation

```bash
# 1. Clone the repository
git clone https://github.com/your-repo/geekmagic-weather-clock.git
cd geekmagic-weather-clock

# 2. Create virtual environment and install dependencies
python3 -m venv venv
venv/bin/pip install -r requirements.txt

# 3. Edit configuration
nano config.json
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
  }
}
```

| Key | Description | Default |
|-----|-------------|---------|
| `device_ip` | GeekMagic device IP address | `192.168.219.119` |
| `city` | City name for weather (OpenWeatherMap format) | `Pyeongtaek-si` |
| `api_key` | OpenWeatherMap API Key | `""` (uses dummy data if empty) |
| `temp_unit` | Temperature unit: `metric` (°C) or `imperial` (°F) | `metric` |
| `time_format` | Clock format: `24h` or `12h` | `24h` |
| `refresh_interval_sec` | Display update interval in seconds | `60` |
| `weather_interval_min` | Weather API fetch interval in minutes | `10` |
| `web_port` | Web config UI port | `8080` |
| `night_mode` | Suppress pushes during specified hours | disabled |

### Usage

**Weather Clock Service (main)**
```bash
venv/bin/python3 main.py
```
Web config UI available at: `http://[raspberry-pi-ip]:8080`

**CPU Monitor (standalone)**
```bash
venv/bin/python3 cpu_monitor.py
```
Cycles: CPU stats for 10 seconds → original device theme for 50 seconds → repeat  
Stop with `Ctrl+C` (restores original theme automatically)

**Display connection test**
```bash
venv/bin/python3 test_display.py
```

### Project Structure

```
├── main.py                  # Entry point (weather clock + web UI)
├── cpu_monitor.py           # Standalone CPU monitor
├── test_display.py          # Display connection test
├── config.json              # Configuration file
├── requirements.txt
├── install.sh               # systemd service registration script
├── src/
│   ├── weather_api.py       # OpenWeatherMap API integration
│   ├── image_generator.py   # Pillow-based 240×240 image renderer
│   ├── push_client.py       # GeekMagic HTTP API client
│   ├── scheduler.py         # Periodic update scheduler
│   └── web_config.py        # Flask web config server
├── templates/
│   └── index.html           # Web config UI
├── cache/                   # Weather JSON cache + current image
└── logs/                    # Runtime logs
```

### Key Device API Endpoints

| Endpoint | Description |
|----------|-------------|
| `POST /doUpload?dir=/image/` | Upload image (multipart/form-data) |
| `GET /set?theme=N` | Switch theme (1=weather clock, 3=photo album) |
| `GET /set?theme=3&img=/image//filename` | Switch theme + set image atomically (prevents flash) |
| `GET /app.json` | Query current theme |
| `GET /img.json` | Query current display image path |
| `GET /v.json` | Query firmware version |

> **Note**: The `requests` library cannot be used due to an ESP8266 firmware bug that sends duplicate `Content-Length` headers. This project uses `http.client` directly.

### Known Limitations

- External images can only be displayed in Photo Album mode (`theme=3`)
- The device always references `weather_clock.jpg` internally regardless of the uploaded filename
- `img.json` periodically resets to the internal saved path; overwriting the file content itself is required

### License

MIT

---

*Last updated: 2026-05-08*
