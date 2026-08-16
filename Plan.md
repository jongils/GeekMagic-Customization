# GeekMagic SmallTV Ultra — 디스플레이 프로토콜 계획

## 방향

라즈베리 파이가 모든 콘텐츠를 생성하고, ESP8266은 수신해서 표시만 한다.  
세 가지 전송 방식을 단계적으로 구현한다.

```
Raspberry Pi                 ESP8266 (SmallTV Ultra)
┌──────────────────┐         ┌───────────────────────────┐
│ 콘텐츠 생성      │  HTTP   │ 수신 → TFT 표시           │
│ (Python)         │ ──────► │                           │
│                  │         │ 디폴트: 시계 (clock_theme) │
└──────────────────┘         └───────────────────────────┘
```

---

## 디스플레이 모드

| 모드 | 진입 방법 | 설명 |
|------|-----------|------|
| `clock` | 기본값 / `GET /mode?set=clock` | 1초 갱신 디지털 시계 |
| `draw`  | `POST /draw` | JSON 드로잉 커맨드 렌더링 |
| `jpeg`  | `POST /display` or `/doUpload` | JPEG 이미지 표시 |

timeout 파라미터를 지정하면 N초 후 자동으로 clock 모드로 복귀.

---

## Phase 1 — 드로잉 커맨드 ✅ (구현 완료)

### 엔드포인트

`POST /draw`  Body: JSON

```json
{
  "clear":   true,
  "bg":      "black",
  "timeout": 30,
  "elements": [
    {"type": "text",   "y": 100, "text": "Hello", "size": 3, "color": "cyan",  "align": "center"},
    {"type": "rect",   "x": 10,  "y": 10, "w": 220, "h": 50, "color": "blue"},
    {"type": "line",   "x1": 0,  "y1": 75, "x2": 240, "y2": 75, "color": "grey"},
    {"type": "hline",  "x": 20,  "y": 190, "len": 200, "color": "white"},
    {"type": "vline",  "x": 120, "y": 80,  "len": 80,  "color": "white"},
    {"type": "circle", "x": 120, "y": 120, "r": 40, "color": "yellow", "fill": false}
  ]
}
```

### 지원 엘리먼트

| type | 필드 | 설명 |
|------|------|------|
| `text` | x/align, y, text, size(1-6), color | 텍스트. align: "left"(기본)/"center"/"right" |
| `rect` | x, y, w, h, color, fill(bool) | 사각형. fill 기본값 true |
| `circle` | x, y, r, color, fill(bool) | 원. fill 기본값 false |
| `line` | x1, y1, x2, y2, color | 직선 |
| `hline` | x, y, len, color | 수평선 |
| `vline` | x, y, len, color | 수직선 |

### 지원 색상

named: `white` `black` `red` `green` `blue` `cyan` `yellow` `orange` `magenta` `grey`  
hex: `"#RRGGBB"`

### Pi 클라이언트

```
pi/draw_client.py
```

```python
from draw_client import SmallTV, text, hline, circle

tv = SmallTV("192.168.219.122")
tv.draw([
    text("center", 100, "14:30", size=4, color="cyan"),
    hline(20, 140, 200, color="grey"),
    text("center", 155, "2025.08.15 FRI", size=1, color="white"),
], timeout=60)
```

---

## Phase 2 — JPEG 푸시 ⬜ (구현 예정)

Pi에서 Pillow/matplotlib로 렌더링한 이미지를 JPEG로 전송.  
사진, 차트, 복잡한 그래픽에 적합.

### 엔드포인트

`POST /display`  — Content-Type: multipart/form-data, field: `file`  
`POST /doUpload` — 레거시 호환 (동일 동작)

### Pi 렌더링 예시

```python
from PIL import Image, ImageDraw, ImageFont
import requests, io

img = Image.new("RGB", (240, 240), "black")
draw = ImageDraw.Draw(img)
draw.text((120, 120), "Hello", fill="white", anchor="mm")

buf = io.BytesIO()
img.save(buf, "JPEG", quality=85)
# POST to /display
```

### 데이터량

- 240×240 JPEG: 약 5–15 KB (드로잉 커맨드 대비 10–50배 크지만 사진/그래픽 가능)

---

## Phase 3 — WebSocket ⬜ (구현 예정)

HTTP 요청마다 연결을 새로 맺는 오버헤드 없이 상시 연결 유지.  
드로잉 커맨드나 JPEG 청크를 실시간으로 스트리밍.

### 예정 엔드포인트

`WS /ws` — WebSocket 업그레이드

### 적합한 상황

- 초당 수회 이상 업데이트가 필요한 실시간 UI
- 센서 데이터 라이브 그래프
- 애니메이션 효과

---

## 공통 엔드포인트 (전 Phase 공통)

| 엔드포인트 | 설명 |
|------------|------|
| `GET /` | 장치 상태 JSON (`{"fw":…,"mode":…,"brt":…,"heap":…}`) |
| `GET /mode?set=clock` | 시계 모드로 복귀 |
| `GET /set?brt=N` | 백라이트 밝기 (0–255) |
| `GET /update` | OTA 업로드 HTML 폼 |
| `POST /update` | OTA 펌웨어 바이너리 수신 |

---

## 표시 영역 주의사항

케이스 베젤로 인해 **y < 75 영역은 가려짐**.  
모든 콘텐츠는 y ≥ 75부터 배치할 것.

```
y=0   ────────── (베젤에 가려짐)
y=75  ────────── 안전 표시 영역 시작
y=240 ──────────
```
