# AGENTS.md — GeekMagic SmallTV Ultra 작업 필수 지식

이 파일은 이 프로젝트를 처음 접하는 AI 에이전트 또는 개발자가 반드시 알아야 할
비자명적 사실들을 모은 것입니다. 코드나 git 이력에서 유추하기 어려운 내용 위주입니다.

---

## 1. 하드웨어 핀 배치 — 직관과 반대인 핀에 주의

| 신호 | GPIO | 함정 |
|------|------|------|
| TFT DC | **0** | GPIO0 는 부팅 스트랩 핀 (LOW = 플래시 모드). DC 용도이지만 부팅 중 HIGH여야 함 |
| TFT RST | **2** | GPIO2 역시 부팅 스트랩 (부팅 중 HIGH 필요). RST용이지만 이름 때문에 혼동 잦음 |
| TFT BL | **5** | PWM 백라이트. `analogWrite(5, N)` |
| TFT MOSI | 13 | 하드웨어 SPI 고정 |
| TFT SCLK | 14 | 하드웨어 SPI 고정 |
| TFT CS | 15 | 부팅 스트랩 (LOW 필요). 디스플레이 측은 실제 미연결 |

> `platformio.ini`의 `[env:esp8266]` build_flags 가 단일 진실 소스.
> `config.h` 나 `User_Setup.h` 에서 핀을 재정의하지 말 것.

---

## 2. TFT 초기화 순서 — 순서가 틀리면 화면이 깜빡임

**반드시 이 순서로 호출해야 합니다.**

```cpp
tft.init();
tft.setRotation(0);
tft.fillScreen(TFT_BLACK);
pinMode(BL_PIN, OUTPUT);     // ← fillScreen 뒤에
analogWrite(BL_PIN, 220);   // ← pinMode 뒤에
```

- `analogWrite` 를 `tft.init()` 이전에 호출하면 화면이 계속 깜빡인다.
- `pinMode` 없이 `analogWrite` 를 호출해도 동일 증상.
- 기준 구현: `firmware/src/display.cpp`의 `displayInit()`.

---

## 3. #include 순서 — WiFi 헤더는 반드시 TFT_eSPI 앞에

```cpp
// 올바른 순서
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <TFT_eSPI.h>           // ← 반드시 WiFi 헤더 뒤에
```

TFT_eSPI 가 먼저 포함되면 `<time.h>` 가 먼저 당겨지고,
이후 `ESP8266WebServer` 의 `ETagFunction` 타입이 `std::nullptr_t` 와 충돌한다.

```
error: cannot convert 'std::nullptr_t' to 'ETagFunction' {aka 'int'}
```

WiFi 기능을 전혀 쓰지 않는 파일에서는 순서 무관.

---

## 4. OTA 업데이트 — espota 프로토콜은 동작 안 함

PlatformIO 내장 espota(`upload_protocol = espota`)는 이 장치에서 "No Answer" 오류.
항상 `curl` 을 사용한다:

```bash
curl -F "image=@.pio/build/esp8266/firmware.bin" http://192.168.219.122/update
```

`platformio.ini`의 `esp8266_ota` 환경은 편의 설정으로 남겨두었으나,
실제 OTA는 위 curl 명령이 유일하게 검증된 방법.

---

## 5. 안전 표시 영역 — 4면 베젤 실측값

`diag_bezel` 사각형 테스트(5px 간격)로 측정한 실측 베젤 마진:

| 면 | 마진 | 측정 근거 |
|----|------|-----------|
| 상단 | **5px** | CYAN(s=0)이 경계선, 여유 5px 적용 |
| 하단 | **10px** | GREEN(s=10)이 첫 번째 완전 표시 |
| 좌측 | **5px** | YELLOW(s=5)가 첫 번째 완전 표시 |
| 우측 | **5px** | YELLOW(s=5)가 첫 번째 완전 표시 |

안전 표시 영역: **x 5~234, y 5~229** (230×225 px)

`firmware/src/config.h`에 상수로 정의됨:
```c
BEZEL_TOP=5, BEZEL_LEFT=5, BEZEL_RIGHT=5, BEZEL_BOTTOM=10
SAFE_X=5, SAFE_Y=5, SAFE_W=230, SAFE_H=225, SAFE_X2=234, SAFE_Y2=229
```

> ⚠ 이전 기록의 "y < 75" 는 틀린 값입니다. 위 실측값을 사용하세요.

---

## 6. 보드 6핀 시리얼 커넥터 핀 순서

```
핀 #  신호     연결 대상
1     GND      Pi Pin 6 (GND)
2     TX       Pi Pin 10 (GPIO15 / RXD) ← 교차 연결
3     RX       Pi Pin 8 (GPIO14 / TXD)  ← 교차 연결
4     VCC      외부 3.3V 전원 (Pi Pin 1은 전류 부족 가능)
5     GPIO0    플래시 모드 시 PIN 1(GND)에 단락
6     RST      오픈 (선택)
```

핀맵 이미지: `firmware/docs/pinmap.svg`

---

## 7. 시리얼 플래시 절차

```bash
# 1. GPIO0(PIN 5) → GND(PIN 1) 단락
# 2. 장치 전원 투입 (플래시 모드 진입)
# 3. 업로드 실행
cd firmware
~/.platformio/penv/bin/pio run -e esp8266 -t upload --upload-port /dev/serial0
# 4. 업로드 완료 후 단락 해제
# 5. 전원 재투입 (정상 부팅)
```

- 업로드 속도: `460800` 고정 (921600은 타임아웃)
- 시리얼 포트: `/dev/serial0` (Pi 하드웨어 UART)
- 업로드 전 `pio device monitor` 종료 필수 (포트 점유)

---

## 8. 빌드 환경

| 환경 | 용도 |
|------|------|
| `esp8266` | 메인 펌웨어 빌드 + 시리얼 플래시 |
| `esp8266_ota` | 메인 펌웨어 OTA (실제로는 curl 사용) |
| `esp8266_diag` | 핀 진단 유틸 (`diag/`) |
| `esp8266_bezel` | 베젤 경계 테스트 (`diag_bezel/`) |
| `esp8266_bezel_ota` | 베젤 펌웨어 OTA 빌드 |

`esp8266_bezel` 은 `extends = env:esp8266` 로 모든 TFT 빌드 플래그를 상속.
새 진단 환경을 추가할 때 동일 패턴을 사용하면 빌드 플래그 중복을 막을 수 있다.

---

## 9. WiFi 없는 펌웨어는 절대 플래시하지 말 것

OTA 복구 엔드포인트(`/update`)가 없으면 시리얼 플래시만이 유일한 복구 수단.
테스트·진단 펌웨어라도 반드시 아래를 포함한다:

```cpp
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
// ...
updater.setup(&server);  // /update 마운트
server.begin();
```

저장된 WiFi 크리덴셜이 없을 경우를 대비해 `WiFi.begin()` (크리덴셜 없이) 만으로 충분;
AP 폴백은 베젤 테스트처럼 단순한 펌웨어에는 불필요.

---

## 10. 장치 정보

| 항목 | 값 |
|------|----|
| IP | `192.168.219.122` (DHCP, 변동 가능) |
| 시리얼 포트 | `/dev/serial0` |
| MCU | ESP8266MOD (ESP-12F) @ 160MHz |
| 디스플레이 | ST7789V, 240×240 IPS |
| 플래시 | 4MB (dio 모드) |
| OTA 엔드포인트 | `POST http://<IP>/update` |
| 상태 확인 | `GET http://<IP>/` |
