# GeekMagic SmallTV Ultra — Pi 구동 디스플레이 터미널

GeekMagic SmallTV Ultra(ESP8266, 1.54" 240×240 IPS)를 **라즈베리 파이가 구동하는 디스플레이 터미널**로 변환한 프로젝트입니다.

## 아키텍처

```
Raspberry Pi (weather-clock.service)
┌─────────────────────────────────────────────────────────┐
│  main.py → WeatherClockScheduler                        │
│                                                         │
│  ┌─ cpu-monitor thread ─────────────────────────────┐   │
│  │  CPU 이미지 생성 → POST /doUpload               │   │ ──► ESP8266 #1
│  │  30초 표시 / 30초 내장 테마 반복                │   │    192.168.219.110
│  └──────────────────────────────────────────────────┘   │
│                                                         │
│  ┌─ temp-push thread ───────────────────────────────┐   │
│  │  /sys/.../temp 읽기 → POST /temp                 │   │ ──► ESP8266 #2
│  │  30초 주기                                       │   │    192.168.219.122
│  └──────────────────────────────────────────────────┘   │
│                                                         │
│  ┌─ scheduler thread ───────────────────────────────┐   │
│  │  날씨 이미지 생성 → POST /doUpload               │   │ ──► ESP8266 #1
│  │  60초마다                                        │   │    192.168.219.110
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘

pi/draw_client.py  →  일회성 드로잉 커맨드 전송 (개발·테스트용)
```

Pi가 JSON 드로잉 커맨드 또는 JPEG를 HTTP로 전송하면 ESP8266이 즉시 화면에 표시합니다.  
Pi 쪽 코드만 바꾸면 펌웨어 업데이트 없이 콘텐츠를 자유롭게 변경할 수 있습니다.

## 문서

| 문서 | 내용 |
|------|------|
| [firmware/README.md](firmware/README.md) | 하드웨어 스펙, HTTP API, 빌드·OTA 방법 |
| [firmware/DEVELOPMENT.md](firmware/DEVELOPMENT.md) | 개발 환경 구축 (PlatformIO, Pi UART, 배선) |
| [firmware/IMPLEMENTATION.md](firmware/IMPLEMENTATION.md) | 모듈별 코드 구조 및 구현 현황 |
| [Plan.md](Plan.md) | 디스플레이 프로토콜 3단계 구현 계획 |

---

## Pi 서비스 (`weather-clock.service`)

### 디렉토리 구조

```
weather-clock/
├── main.py              ← 서비스 진입점 (Flask + Scheduler 시작)
├── cpu_monitor.py       ← 단독 실행용 CPU 모니터 (standalone)
├── config.json          ← 설정 파일 (gitignore, 수동 관리)
├── push_client.py       ← 온도 단독 전송 CLI (python3 push_client.py --once)
├── src/
│   ├── push_client.py   ← GeekMagicClient + push_temp_to() + get_cpu_temp()
│   ├── scheduler.py     ← WeatherClockScheduler (모든 스레드 관리)
│   ├── cpu_image.py     ← CPU 통계 → 240×240 JPEG 렌더러
│   ├── image_generator.py ← 날씨+시계 → 240×240 JPEG 렌더러
│   ├── weather_api.py   ← OpenWeatherMap API 래퍼
│   ├── web_config.py    ← Flask 웹 설정 UI (포트 8080)
│   ├── slideshow.py     ← 로컬 폴더 슬라이드쇼
│   ├── console_image.py ← 명령어 출력 이미지 렌더러
│   └── camera_client.py ← USB 카메라 슬라이드쇼
└── pi/
    └── draw_client.py   ← 일회성 드로잉 커맨드 클라이언트 (개발용)
```

### config.json 주요 항목

```json
{
  "device_ip":  "192.168.219.110",   // CPU 이미지·날씨 Push 대상 (ESP8266 #1)
  "cpu_monitor": {
    "enabled":       true,
    "show_sec":      30,              // CPU 화면 표시 시간
    "rest_sec":      30,              // 내장 테마 표시 시간
    "restore_theme": 1                // 복원할 내장 테마 번호 (0=날씨화면)
  },
  "temp_push": {
    "enabled":      true,
    "device_ip":    "192.168.219.122", // 온도 전송 대상 (ESP8266 #2)
    "interval_sec": 30                 // 전송 주기 (초)
  }
}
```

### 스레드 구성

| 스레드 이름 | 동작 | 대상 |
|-------------|------|------|
| `scheduler` | 60초마다 날씨+시계 이미지 Push | 192.168.219.110 |
| `cpu-monitor` | 30초 CPU 이미지 표시 / 30초 내장 테마 반복 | 192.168.219.110 |
| `temp-push` | 30초마다 Pi CPU 온도 `POST /temp` 전송 | 192.168.219.122 |

### 서비스 관리

```bash
# 상태 확인
sudo systemctl status weather-clock.service

# 로그 확인
journalctl -u weather-clock.service -f

# 재시작 (config.json 변경 후)
sudo systemctl restart weather-clock.service
```

### 온도 단독 전송 (테스트)

```bash
# 한 번만 전송
python3 push_client.py --once --ip 192.168.219.122

# 30초 주기 반복 (서비스와 별개로 실행 시)
python3 push_client.py --interval 30 --ip 192.168.219.122
```

---

## 빠른 시작

```bash
# Pi에서 드로잉 커맨드 전송 (개발·테스트용)
cd pi
python3 draw_client.py
```

```bash
# 또는 curl로 직접 전송
curl -X POST http://<장치IP>/draw \
  -H "Content-Type: application/json" \
  -d '{"elements":[{"type":"text","y":100,"text":"Hello!","size":3,"color":"cyan","align":"center"}]}'
```
