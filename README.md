# GeekMagic SmallTV Ultra — Pi 구동 디스플레이 터미널

GeekMagic SmallTV Ultra(ESP8266, 1.54" 240×240 IPS)를 **라즈베리 파이가 구동하는 디스플레이 터미널**로 변환한 프로젝트입니다.

## 아키텍처

```
Raspberry Pi                    ESP8266 (SmallTV Ultra)
┌─────────────────────┐         ┌────────────────────────┐
│ 콘텐츠 생성         │  HTTP   │ 수신 즉시 TFT에 표시   │
│ pi/draw_client.py   │ ──────► │                        │
│ (시계·알림·그래프 등) │        │ 디폴트: 디지털 시계    │
└─────────────────────┘         └────────────────────────┘
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

## 빠른 시작

```bash
# Pi에서 드로잉 커맨드 전송
cd pi
python3 draw_client.py
```

```bash
# 또는 curl로 직접 전송
curl -X POST http://<장치IP>/draw \
  -H "Content-Type: application/json" \
  -d '{"elements":[{"type":"text","y":100,"text":"Hello!","size":3,"color":"cyan","align":"center"}]}'
```
