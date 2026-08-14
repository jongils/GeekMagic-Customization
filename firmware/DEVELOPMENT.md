# 개발 환경 구축 가이드

GeekMagic SmallTV Ultra 커스텀 펌웨어를 빌드·플래시·디버깅하기 위한 환경 설정 가이드입니다.

---

## 개발 호스트 환경

| 항목 | 사용 버전 |
|------|-----------|
| 호스트 | Raspberry Pi 5 Model B |
| OS | Raspberry Pi OS (Debian 12, aarch64) |
| Python | 3.13 |
| PlatformIO Core | 6.1.19 |
| 프레임워크 | Arduino for ESP8266 (`espressif8266 @ 4.2.1`) |

> macOS / Linux PC에서도 동일하게 동작합니다. Windows는 PlatformIO IDE(VSCode) 사용을 권장합니다.

---

## 1. 필수 소프트웨어 설치

### Python 3

```bash
sudo apt update
sudo apt install python3 python3-pip python3-venv
```

### PlatformIO Core (CLI)

```bash
# 공식 설치 스크립트 사용
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core-installer/master/get-platformio.py -o get-platformio.py
python3 get-platformio.py
```

설치 후 PATH에 추가합니다:

```bash
echo 'export PATH="$HOME/.platformio/penv/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

설치 확인:

```bash
pio --version
# PlatformIO Core, version 6.x.x
```

---

## 2. Raspberry Pi UART 설정

Raspberry Pi의 GPIO 하드웨어 UART를 ESP8266 플래싱에 사용합니다.

### 2-1. UART 활성화

`/boot/firmware/config.txt` (또는 `/boot/config.txt`)에 다음 줄이 있는지 확인합니다:

```
enable_uart=1
```

없으면 추가하고 재부팅합니다:

```bash
echo "enable_uart=1" | sudo tee -a /boot/firmware/config.txt
sudo reboot
```

### 2-2. 시리얼 콘솔 비활성화 확인

`/boot/firmware/cmdline.txt`에 `console=serial0,115200` 항목이 **없어야** 합니다.  
있으면 해당 항목만 삭제하고 저장한 뒤 재부팅합니다.

올바른 cmdline.txt 예시:

```
console=tty1 root=PARTUUID=xxxxxxxx-02 rootfstype=ext4 fsck.repair=yes rootwait quiet splash
```

### 2-3. dialout 그룹 추가

```bash
sudo usermod -aG dialout $USER
# 로그아웃 후 재로그인 필요
```

확인:

```bash
groups
# ... dialout ...
```

### 2-4. 시리얼 장치 확인

```bash
ls -la /dev/serial0
# lrwxrwxrwx ... /dev/serial0 -> ttyAMA0
```

`/dev/serial0` → `ttyAMA0` 심링크가 있으면 정상입니다.

---

## 3. 하드웨어 연결 (Pi ↔ ESP8266)

### 핀 배치

| Raspberry Pi 5 핀 | GPIO | ESP8266 핀 |
|-------------------|------|------------|
| Pin 8 (TXD) | GPIO14 | RX |
| Pin 10 (RXD) | GPIO15 | TX |
| Pin 6 (GND) | GND | GND |
| — | — | VCC → 3.3V 또는 외부 전원 |

> **주의:** Pi의 TX → ESP의 RX, Pi의 RX → ESP의 TX 로 교차 연결합니다.

### 플래시 모드 진입용 추가 연결

ESP8266을 플래시 모드로 부팅하려면 `GPIO0`을 `GND`에 연결한 상태에서 전원을 넣어야 합니다.

```
ESP8266 GPIO0 ──────────┐
ESP8266 GND  ──────────┘  (점퍼 또는 누름 스위치)
```

### 연결 다이어그램

```
Raspberry Pi 5               ESP8266 (ESP-12F)
┌──────────────┐             ┌──────────────┐
│ GPIO14 (TXD) ├────────────►│ RX           │
│ GPIO15 (RXD) │◄────────────┤ TX           │
│ GND          ├─────────────┤ GND          │
└──────────────┘             │ GPIO0 ──┐    │
                             │ GND  ───┘    │ ← 플래시 시에만
                             │ VCC → 외부   │
                             └──────────────┘
```

---

## 4. 저장소 클론 및 빌드

```bash
git clone https://github.com/jongils/GeekMagic-Customization.git
cd GeekMagic-Customization/firmware

# 의존 라이브러리 자동 설치 후 빌드
pio run -e esp8266
```

첫 빌드 시 PlatformIO가 자동으로 다음을 설치합니다:

- `espressif8266` 플랫폼 (Arduino 코어)
- TFT_eSPI, ArduinoJson, JPEGDEC, NTPClient, WiFiManager

---

## 5. 펌웨어 플래시 (시리얼)

### 절차

```
1. ESP8266의 GPIO0 → GND 연결
2. ESP8266 전원 투입 (또는 RST 핀 재설정)
3. 아래 명령 실행
4. 플래시 완료 후 GPIO0 → GND 연결 해제
5. ESP8266 전원 재투입 → 정상 부팅
```

```bash
pio run -e esp8266 -t upload --upload-port /dev/serial0
```

### 파일시스템 업로드 (LittleFS)

설정 파일이나 초기 데이터를 `data/` 폴더에 두고 LittleFS로 올릴 때:

```bash
pio run -e esp8266 -t uploadfs --upload-port /dev/serial0
```

### 업로드 속도 주의사항

`platformio.ini`에 `upload_speed = 460800`으로 설정되어 있습니다.  
`921600`은 Pi ↔ ESP8266 간 통신에서 타임아웃이 발생하므로 사용하지 않습니다.

---

## 6. OTA 펌웨어 업데이트 (WiFi)

장치가 WiFi에 연결된 이후에는 시리얼 연결 없이 업데이트할 수 있습니다.

```bash
# 빌드
pio run -e esp8266

# OTA 업로드
curl -F "image=@.pio/build/esp8266/firmware.bin" http://<장치IP>/update
```

PlatformIO OTA 환경도 설정되어 있습니다:

```bash
# platformio.ini의 [env:esp8266_ota] 사용
pio run -e esp8266_ota -t upload
```

> `upload_port`를 장치 IP로 맞춰야 합니다 (`platformio.ini`의 `esp8266_ota` 섹션 참고).

---

## 7. 시리얼 모니터

```bash
pio device monitor --port /dev/serial0 --baud 115200
```

> **주의:** 시리얼 모니터가 열려 있으면 동일 포트로 플래시할 수 없습니다.  
> 플래시 전 모니터를 반드시 종료하세요 (`Ctrl+C`).  
> 백그라운드 프로세스가 포트를 점유 중이면:
> ```bash
> fuser /dev/serial0       # PID 확인
> kill <PID>               # 종료
> ```

---

## 8. 프로젝트 구조

```
firmware/
├── platformio.ini          ← 빌드 환경 설정 (핀 배치, 드라이버, 라이브러리)
├── src/
│   ├── main.cpp            ← 진입점 (setup / loop)
│   ├── config.h            ← 전역 상수 (핀, 테마 번호, NTP 등)
│   ├── display.cpp/.h      ← TFT_eSPI 래퍼
│   ├── http_server.cpp/.h  ← HTTP API 서버 (포트 80)
│   ├── filesystem.cpp/.h   ← LittleFS 파일 입출력
│   ├── jpeg_display.cpp/.h ← JPEGDEC → TFT 스트리밍
│   ├── clock_theme.cpp/.h  ← Theme 4·5·6: 실시간 시계
│   └── weather_theme.cpp/.h← Theme 1·2·7: 날씨+시계
├── diag/
│   └── main.cpp            ← 핀 탐색 진단 유틸리티 (별도 env)
└── data/                   ← LittleFS 초기 탑재 파일 (필요 시)
```

### 빌드 환경 (`platformio.ini`)

| 환경 이름 | 용도 |
|-----------|------|
| `esp8266` | 일반 빌드 + 시리얼 플래시 |
| `esp8266_ota` | OTA 빌드 + WiFi 업로드 |
| `esp8266_diag` | 핀 진단 펌웨어 (`diag/` 소스 사용) |

---

## 9. 라이브러리 목록

| 라이브러리 | 버전 | 용도 |
|------------|------|------|
| Bodmer/TFT_eSPI | ^2.5.43 | ST7789V 디스플레이 드라이버 |
| bblanchon/ArduinoJson | ^7.3.1 | JSON 파싱 |
| bitbank2/JPEGDEC | ^1.2.9 | JPEG 디코딩 (프레임버퍼 없는 스트리밍) |
| arduino-libraries/NTPClient | ^3.2.1 | NTP 시각 동기화 |
| tzapu/WiFiManager | ^2.0.17 | 첫 실행 시 AP 모드 WiFi 설정 |

---

## 10. 자주 발생하는 문제

| 증상 | 원인 | 해결 |
|------|------|------|
| `Timed out waiting for packet header` | 업로드 속도 너무 높음 | `upload_speed = 460800` 확인 |
| `device reports readiness to read but returned no data` | 시리얼 포트 이미 점유 중 | `fuser /dev/serial0` 후 해당 PID 종료 |
| 업로드 후 화면 아무것도 없음 | GPIO0→GND 연결 해제 후 재부팅 안 함 | GPIO0 분리 후 전원 재투입 |
| WiFi 연결 안 됨 | 저장된 SSID 없음 | "GeekMagic-Setup" AP로 접속해 설정 |
| `/update` OTA 실패 | espota 프로토콜 사용 시도 | `curl -F "image=@firmware.bin" http://IP/update` 사용 |
| 한글 텍스트 깨짐 | TFT_eSPI 내장 폰트는 ASCII 전용 | 영문 또는 숫자만 사용 |
