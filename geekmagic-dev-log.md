# GeekMagic SmallTV Ultra — 개발 과정 전체 기록

> **기간**: 2026년 5월  
> **목표**: GeekMagic SmallTV Ultra 장치에 Raspberry Pi 5를 연동하여 날씨/시계 및 시스템 모니터링 화면을 표시  

---

## 1. 하드웨어 구성

| 장치 | 스펙 | 역할 |
|------|------|------|
| GeekMagic SmallTV Ultra | ESP8266 (ESP-12F), TFT LCD 240×240, Flash 4MB | 표시 장치 |
| Raspberry Pi 5 | Python 3.13 | 이미지 생성 및 Push 서버 |

### 장치 식별 과정
- 보라색 PCB에 ESP-12F 모듈 + AMS1117 레귤레이터 탑재 확인
- GeekMagic 공식 GitHub 분석 → **Ultra = ESP8266**, PRO = ESP32
- 펌웨어 버전: `SmallTV-Ultra / Ultra-V9.0.41`
- 장치 IP: 로컬 네트워크 내 할당 IP (`config.json`의 `device_ip`에 설정)

---

## 2. 전략 수립

### 초기 계획 (펌웨어 커스터마이징)
- ESP8266 위에서 직접 날씨/시계 펌웨어 작성 검토
- **포기 이유**: ESP8266 RAM 80KB 제약, OTA 실패 시 벽돌 위험, 개발 기간 2~4주

### 채택 전략: HTTP Image Push
- Medium 글(@jungkim) 참고: GeekMagic 장치에 HTTP API 내장 확인
- Raspberry Pi에서 이미지 생성 → 장치에 HTTP로 Push
- **장점**: 공식 펌웨어 유지, 안정적, Python으로 자유로운 확장

```
Raspberry Pi 5
  ├── 이미지 생성 (Pillow)
  ├── HTTP Push → GeekMagic 장치
  ├── 날씨 API (OpenWeatherMap)
  └── 웹 설정 UI (Flask)
```

---

## 3. 환경 구성

### 3-1. pip 직접 설치 실패
```bash
pip3 install pillow requests flask schedule pytz psutil
# ❌ 오류: externally-managed-environment (PEP 668)
# Python 3.13 + Raspberry Pi OS Bookworm 정책으로 시스템 pip 차단
```

### 3-2. 해결 — venv 가상환경
```bash
python3 -m venv ~/Documents/GeekMagic/weather-clock/venv
venv/bin/pip install pillow requests flask schedule pytz psutil
# ✅ 성공
```

### 3-3. install.sh 경로 하드코딩 문제
- `install.sh`에 `PROJECT_DIR="$HOME/weather-clock"` 하드코딩
- 실제 경로: 저장소 클론 위치 기준 자동 감지
- systemd 서비스가 `activating` 상태에서 반복 실패
- **해결**: `PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"` 로 자동 감지

---

## 4. 장치 HTTP API 탐색

### 4-1. 기본 API 확인
```
GET  http://192.168.x.x/                       → 200 (웹 콘솔)
POST http://192.168.x.x/doUpload?dir=/image/   → 이미지 업로드
GET  http://192.168.x.x/set?img=/image//파일명  → 화면 표시
GET  http://192.168.x.x/update                 → OTA 펌웨어 업데이트 페이지
```

### 4-2. requests 라이브러리 오류
```
urllib3.exceptions.InvalidHeader:
Content-Length contained multiple unmatching values (4237, 2082)
```
- **원인**: ESP8266 펌웨어 버그 — HTTP 응답에 `Content-Length` 헤더를 중복으로 전송
- **해결**: `requests` 대신 `http.client` 직접 사용 (헤더 검증 느슨함)

```python
# ❌ requests 사용 시 InvalidHeader 오류
requests.post(url, files={"file": (filename, f, "image/jpeg")})

# ✅ http.client 직접 사용
conn = http.client.HTTPConnection(host, timeout=10)
conn.request("POST", path, body=body, headers={...})
```

### 4-3. 내부 상태 JSON API 확인
```json
GET /app.json → {"theme":3}
GET /img.json → {"img":"/image//weather_clock.jpg"}
GET /v.json   → {"m":"SmallTV-Ultra","v":"Ultra-V9.0.41"}
GET /brt.json → {"brt":"15"}
```

---

## 5. 이미지 표시 문제 해결 과정 (핵심)

### 5-1. 업로드 성공했는데 화면이 안 바뀜 ❌
- 업로드 HTTP 200, `/set?img=` HTTP 200 → 화면 변화 없음
- **원인**: 장치가 `Photo Album (theme=3)` 모드일 때만 외부 이미지를 표시
- **해결**: `/set?theme=3` 먼저 호출 후 이미지 표시 → ✅ 성공

---

### 5-2. CPU/원래 테마 전환 시 이전 이미지가 깜박이는 문제 ❌

cpu_monitor.py에서 CPU 이미지(10초) ↔ 원래 테마(50초)를 교대로 전환하는 중,
테마 전환 순간마다 이전에 테스트했던 이미지가 짧게 나타나는 현상.

**시도 1 — 파일 삭제**
```python
esp_get(f"/delete?f=/image//{FILENAME}")
```
결과: 삭제는 되는데 `theme=3` 전환 시 여전히 이전 이미지 재표시 ❌

**시도 2 — 이미지 업로드 순서 변경 + sleep**
```python
esp_upload(OUTPUT_PATH)       # 먼저 업로드
esp_get("/set?theme=3")
time.sleep(0.5)               # ← 0.5초 동안 이전 이미지 표시됨
esp_get(f"/set?img=/image//{FILENAME}")
```
결과: `theme=3` 전환과 `img=` 지정 사이 sleep 동안 여전히 깜박임 ❌

---

### 5-3. 근본 원인 파악

**핵심 발견 1**: 장치가 실제로 사용하는 파일명은 **`weather_clock.jpg` 고정**
- 어떤 파일명으로 업로드해도 `img.json`은 항상 `weather_clock.jpg`로 리셋됨
- `weather_clock.jpg` 파일 자체를 덮어써야만 화면이 변경됨

**핵심 발견 2**: `theme=3`으로 전환되는 순간 장치가 `img.json`에 저장된 마지막 이미지를 즉시 표시
- `/set?theme=3` 호출 후 어떤 명령을 보내도 그 사이 타이밍 갭이 발생

**핵심 발견 3 (최종 확정)**: `/set?theme=3&img=...` 조합 쿼리는 **ESP8266 펌웨어가 `img=` 파라미터를 무시함**
- HTTP 200을 반환하지만 실제로는 `theme=3` 전환만 처리되고 이미지는 변경되지 않음
- 화면이 "멈추는 듯한 현상"(theme 전환)은 있으나 이미지가 바뀌지 않는 이유
- 날씨 화면은 이전 이미지와 새 이미지가 동일 파일(`weather_clock.jpg`)이라 문제를 못 느꼈음
- CPU 이미지 전환 시 명확히 증상 확인

---

### 5-4. 최종 해결 방법 ✅

**3단계 분리 + sleep 제거**:
1. 이미지 업로드 (`POST /doUpload`) → 파일 먼저 전송
2. 테마 전환 (`GET /set?theme=3`) → 포토앨범 모드 활성화
3. 즉시 이미지 지정 (`GET /set?img=...`) → sleep 없이 연속 실행

```python
# ✅ 올바른 방식
status, _ = self._post_file("/doUpload?dir=/image/", image_path, UPLOAD_FILENAME)
status2, _ = self._get("/set?theme=3")          # 테마 전환
status3, _ = self._get(f"/set?img={UPLOAD_PATH}")  # 즉시 이미지 지정 (sleep 없음)
```

**포인트**:
- sleep 제거로 theme=3 전환과 img 지정 사이 간격이 네트워크 왕복 1회(~50ms) 수준으로 단축
- 이미지를 먼저 업로드하여 theme=3 전환 시 장치가 올바른 파일을 즉시 참조
- 내장 테마 복원은 이미지 조작 없이 `GET /set?theme=N` 만으로 충분

```python
# 내장 테마 복원 (이미지 업로드 불필요)
def set_theme(self, theme: int) -> bool:
    status, _ = self._get(f"/set?theme={theme}")
    return status == 200
```

---

## 5-5. cpu_monitor.py + main.py 동시 실행 충돌 문제 ❌→✅

### 문제 발견

`cpu_monitor.py`를 단독 실행하는데도 여전히 이전 이미지가 깜박이는 현상이 지속됨.
원인 조사 중 **`main.py`가 systemd 서비스로 백그라운드에서 함께 실행 중**임을 확인.

```
[main.py 스케줄러]    매 60초 → theme=3 + weather_clock.jpg 덮어쓰기
[cpu_monitor.py]     테마 복원 → /set?theme=1
                          ↑ main.py가 60초 안에 다시 theme=3으로 강제 전환
```

두 프로세스가 서로의 테마 설정을 계속 덮어쓰는 레이스 컨디션.

### 해결 — 스케줄러 통합

`cpu_monitor.py`를 `main.py`의 스케줄러 안으로 흡수. 단일 프로세스가 날씨 Push와 CPU Push를 조율.

```
[scheduler thread]   날씨 이미지 60초마다 Push
                       └─ restore_theme != 0 이면 자동 skip
                       └─ _showing_cpu=True 이면 자동 skip

[cpu-monitor thread] rest_sec 대기 → CPU Push → show_sec 대기 → 테마 복원 → 반복
                       └─ threading.Lock으로 날씨 Push와 충돌 방지
```

```python
# src/scheduler.py 핵심 구조
class WeatherClockScheduler:
    def __init__(self, ...):
        self._push_lock   = threading.Lock()
        self._showing_cpu = False

    def _update_display(self):
        if self._restore_theme() != 0:  # 내장 테마 모드이면 날씨 Push skip
            return
        if self._showing_cpu:           # CPU 표시 중이면 skip
            return
        with self._push_lock:
            ...날씨 이미지 Push...

    def _cpu_monitor_loop(self):
        while self._running:
            # 매 사이클 시작 시 config 재로드 (웹 UI 변경 즉시 반영)
            cfg = self.config.get("cpu_monitor", {})
            show_sec = cfg.get("show_sec", 10)
            rest_sec = cfg.get("rest_sec", 50)
            ...
            sleep(rest_sec)             # 내장 테마 표시 기간 대기
            self._showing_cpu = True
            with self._push_lock:
                ...CPU 이미지 Push...
            sleep(show_sec)
            self._showing_cpu = False
            self.push_client.set_theme(restore_theme)  # 내장 테마 복원
```

---

## 5-6. 설정 구조 개선 — interval_sec → rest_sec

### 문제
`interval_sec`(전체 주기) 개념이 직관적이지 않아 설정 혼란 발생.

- `interval_sec = 30`, `show_sec = 30` 으로 설정 시:
  - 코드 내부: `rest_sec = max(30, 30+5) - 30 = 5초`
  - 결과: GeekMagic 테마 5초 표시, CPU 30초 표시 → 완전히 역전

### 해결
`interval_sec` 폐기, `rest_sec`(GeekMagic 테마 표시 시간)을 직접 사용.

| 항목 | 이전 | 현재 |
|------|------|------|
| 설정 키 | `interval_sec` (전체 주기) | `rest_sec` (테마 표시 시간) |
| CPU 표시 시간 | `interval_sec - show_sec` | `show_sec` (직접 지정) |
| 테마 표시 시간 | `interval_sec - show_sec` | `rest_sec` (직접 지정) |

```json
// 현재 config 구조
"cpu_monitor": {
  "enabled": true,
  "show_sec": 10,     // CPU 정보 표시 시간 (초)
  "rest_sec": 50,     // GeekMagic 내장 테마 표시 시간 (초)
  "restore_theme": 1  // 복원할 내장 테마 번호 (0=날씨 커스텀 이미지)
}
```

### 웹 UI 개선
- "전환 주기" 단일 필드 → "🖥 GeekMagic 테마 표시 시간 / 📊 CPU 정보 표시 시간" 분리
- 동적 config 반영: 설정 저장 시 `_scheduler.config.update()` 호출 → 서비스 재시작 없이 다음 사이클부터 적용

---

## 6. 최종 파일 구성

```
~/Documents/GeekMagic/weather-clock/
├── main.py                  ← 전체 진입점 (날씨+시계 + CPU 모니터 통합)
├── config.json              ← 설정 파일
├── requirements.txt
├── install.sh               ← systemd 서비스 자동 등록
├── cpu_monitor.py           ← CPU 모니터 standalone (main.py 미실행 시 전용)
├── test_display.py          ← 화면 표시 단위 테스트
├── venv/                    ← Python 가상환경
├── src/
│   ├── weather_api.py       ← OpenWeatherMap 연동
│   ├── image_generator.py   ← Pillow 날씨+시계 이미지 생성
│   ├── cpu_image.py         ← Pillow CPU 모니터 이미지 생성 (공용 모듈)
│   ├── push_client.py       ← GeekMagic HTTP 클라이언트
│   ├── scheduler.py         ← 스케줄러 (날씨+CPU 통합, 스레드 조율)
│   └── web_config.py        ← Flask 설정 서버
├── templates/
│   └── index.html           ← 웹 설정 UI
├── cache/                   ← 날씨 캐시, 이미지 캐시
└── logs/                    ← 실행 로그
```

---

## 7. ESP8266 HTTP API 레퍼런스

### 엔드포인트

| 엔드포인트 | 메서드 | 설명 |
|-----------|--------|------|
| `/doUpload?dir=/image/` | POST | 이미지 업로드 (multipart/form-data) |
| `/set?theme=N` | GET | 테마 전환 |
| `/set?img=/image//파일명` | GET | 표시 이미지 지정 (theme=3 상태에서만 유효) |
| `/set?brt=N` | GET | 밝기 조절 |
| `/app.json` | GET | 현재 테마 확인 |
| `/img.json` | GET | 현재 표시 이미지 경로 |
| `/v.json` | GET | 모델/버전 확인 |
| `/update` | GET/POST | OTA 펌웨어 업데이트 |

> ⚠️ `/set?theme=3&img=...` 조합 쿼리는 HTTP 200을 반환하지만 `img=` 파라미터를 무시함.
> 테마 전환과 이미지 지정은 **반드시 별도 요청**으로 분리해야 함.

### 테마 번호

| 번호 | 이름 | 설명 |
|------|------|------|
| 1 | Weather Clock Today | 날씨+시계 (기본) |
| 2 | Weather Forecast | 날씨 예보 |
| 3 | Photo Album | **외부 이미지 표시 필수 모드** |
| 4 | Time Style 1 | 시간 스타일 |
| 5 | Time Style 2 | 시간 스타일 |
| 6 | Time Style 3 | 시간 스타일 |
| 7 | Simple Weather Clock | 심플 날씨 시계 |

### 주요 주의사항

1. **Content-Length 중복 헤더 버그**: `requests` 사용 불가 → `http.client` 직접 사용
2. **외부 이미지 표시 조건**: 반드시 `theme=3` (Photo Album) 상태여야 함
3. **실제 파일명 고정**: `weather_clock.jpg` — 장치 내부에서 항상 이 이름으로 참조
4. **img.json 자동 리셋**: 파일 내용 자체를 덮어써야 하며, 파일명 변경으로는 적용 불가
5. **조합 쿼리 제한**: `/set?theme=3&img=...` 에서 `img=`는 ESP8266 펌웨어에서 무시됨
6. **내장 테마 복원 시 이미지 불필요**: theme 1~2, 4~7은 장치 자체 렌더링, 이미지 조작 무관
7. **venv 필수**: Python 3.13 + Raspberry Pi OS Bookworm에서 시스템 pip 차단 (PEP 668)

---

## 8. 현재 상태

| 항목 | 상태 | 비고 |
|------|------|------|
| 장치 HTTP API 파악 | ✅ 완료 | 조합 쿼리 버그 포함 전체 동작 파악 |
| venv 환경 구성 | ✅ 완료 | |
| requests 버그 우회 | ✅ 완료 | http.client 직접 사용 |
| 이미지 Push (날씨 화면) | ✅ 완료 | 3단계 분리 push (sleep 없음) |
| CPU 모니터 표시 | ✅ 완료 | 내장 테마 ↔ CPU 이미지 교대 정상 동작 |
| main.py + cpu_monitor.py 충돌 | ✅ 해결 | 스케줄러 통합 |
| 동적 config 반영 | ✅ 완료 | 웹 UI 저장 시 서비스 재시작 없이 다음 사이클 반영 |
| 웹 설정 UI | ✅ 운영 중 | http://[Pi IP]:8080 |
| systemd 서비스 | ✅ 운영 중 | weather-clock.service 자동 시작 |
| 날씨 실데이터 | ⏸ 대기 | OpenWeatherMap API Key 발급 후 적용 필요 |

---

## 9. 참고 자료

| 자료 | URL |
|------|-----|
| GeekMagic 공식 GitHub | https://github.com/GeekMagicClock |
| SmallTV Ultra 펌웨어 | https://github.com/GeekMagicClock/smalltv-ultra |
| Medium 참고 글 (Jung Kim) | https://medium.com/@jungkim/나만의-1인치-모니터링-장치-만들기-ec131bc2b357 |
| OpenWeatherMap API | https://openweathermap.org/api |
| PEP 668 (venv 정책) | https://peps.python.org/pep-0668/ |

---

*최종 업데이트: 2026-05-09*
