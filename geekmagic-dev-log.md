# GeekMagic SmallTV Ultra — 개발 과정 전체 기록

> **기간**: 2026년 5월  
> **목표**: GeekMagic SmallTV Ultra 장치에 Raspberry Pi 5를 연동하여 날씨/시계 및 시스템 모니터링 화면을 표시  
> **작성일**: 2026-05-08

---

## 1. 하드웨어 구성

| 장치 | 스펙 | 역할 |
|------|------|------|
| GeekMagic SmallTV Ultra | ESP8266 (ESP-12F), TFT LCD 240×240, Flash 4MB | 표시 장치 |
| Raspberry Pi 5 | Python 3.13, IP: 192.168.219.116 | 이미지 생성 및 Push 서버 |
| 개발 머신 | Mac | 코드 작성 |

### 장치 식별 과정
- 보라색 PCB에 ESP-12F 모듈 + AMS1117 레귤레이터 탑재 확인
- GeekMagic 공식 GitHub 분석 → **Ultra = ESP8266**, PRO = ESP32
- 펌웨어 버전: `SmallTV-Ultra / Ultra-V9.0.41`
- 장치 IP: `192.168.219.119`

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

### 3-1. 첫 번째 시도 — pip 직접 설치 실패
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
- 실제 경로: `/home/pi5/Documents/GeekMagic/weather-clock`
- systemd 서비스가 `activating` 상태에서 반복 실패
- **해결**: `PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"` 로 자동 감지

---

## 4. 장치 HTTP API 탐색

### 4-1. 기본 API 확인 (성공)
```
GET  http://192.168.219.119/                       → 200 (웹 콘솔)
POST http://192.168.219.119/doUpload?dir=/image/   → 이미지 업로드
GET  http://192.168.219.119/set?img=/image//파일명  → 화면 표시
GET  http://192.168.219.119/update                 → OTA 펌웨어 업데이트 페이지
```

### 4-2. requests 라이브러리 오류 발생
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

**시도 2 — 더미 이미지로 img.json 덮어쓰기**
```python
# 검정 1×1 더미 이미지 → dummy.jpg 업로드 후 경로 교체
```
결과: `theme=3` 전환 시 장치 내부 저장값으로 다시 원래 이미지 표시 ❌

**시도 3 — 이미지 업로드 순서 변경**
```python
# 순서: CPU 업로드 → theme=3 전환 → /set?img= 지정
esp_get("/set?theme=3")
time.sleep(0.2)
esp_upload(OUTPUT_PATH)
esp_get(f"/set?img=/image//{FILENAME}")
```
결과: `theme=3` 전환 직후 0.2초 동안 이전 `weather_clock.jpg`가 잠깐 보임 ❌

**시도 4 — 업로드를 theme 전환보다 먼저**
```python
esp_upload(OUTPUT_PATH)       # 먼저 업로드
esp_get("/set?theme=3")
time.sleep(0.2)
esp_get(f"/set?img=/image//{FILENAME}")
```
결과: 업로드 시점은 해결됐으나 theme=3 전환 순간~/set?img= 사이 0.2초 동안 여전히 깜박임 ❌

**시도 5 — 원래 테마 복원에 이미지 백업/복원 사용**
```python
# 시작 시 weather_clock.jpg 백업
# 복원 시 백업 이미지 재업로드 후 theme=1 전환
```
결과: 백업 시점에 이미 테스트 이미지가 들어있어서 그게 그대로 복원됨 ❌  
근본 원인 해결 불가, 복잡도만 증가

---

### 5-3. 근본 원인 파악

`img.json` 확인:
```json
{"img": "/image//weather_clock.jpg"}
```

**핵심 발견 1**: 장치가 실제로 사용하는 파일명은 **`weather_clock.jpg` 고정**
- 어떤 파일명으로 업로드해도 `img.json`은 항상 `weather_clock.jpg`로 리셋됨
- `weather_clock.jpg` 파일 자체를 덮어써야만 화면이 변경됨

**핵심 발견 2**: `theme=3`으로 전환되는 순간 장치가 `img.json`에 저장된 마지막 이미지를 즉시 표시
- `/set?theme=3` 호출 후 어떤 명령을 보내도 그 사이 1프레임 이상 이전 이미지가 표시됨
- `/set?img=` 를 별도 요청으로 보내면 반드시 이 타이밍 갭이 발생

**핵심 발견 3**: `/set` 엔드포인트는 복합 쿼리 파라미터를 지원함
```bash
# 단일 요청으로 테마 전환 + 이미지 지정 동시 처리 → HTTP 200 OK
GET /set?theme=3&img=/image//weather_clock.jpg
```

---

### 5-4. 최종 해결 방법 ✅

```
CPU 화면 표시 (10초):
  1. weather_clock.jpg = CPU 이미지로 업로드 (theme 전환 전)
  2. /set?theme=3&img=/image//weather_clock.jpg  ← 단일 요청으로 원자적 처리

원래 테마 복원 (50초):
  1. /set?theme=1  ← 장치 내장 테마이므로 이미지 업로드 불필요

종료 시 (Ctrl+C):
  → 자동으로 복원 루틴 실행
```

**포인트**:
- 이미지를 먼저 올려 두면 `theme=3` 전환 시 장치가 이미 새 파일을 읽음
- `theme=3&img=...` 단일 요청으로 테마 전환과 이미지 지정 사이의 타이밍 갭 제거
- `theme=1`은 장치 내장 테마이므로 이미지 백업/복원이 전혀 불필요

```python
def show_cpu():
    esp_upload(OUTPUT_PATH)                              # 1. 먼저 업로드
    esp_get(f"/set?theme=3&img=/image//{FILENAME}")     # 2. 원자적 전환

def restore_theme():
    esp_get(f"/set?theme={ORIG_THEME}")                 # theme=1, 이미지 조작 없음
```

---

## 6. 최종 파일 구성

```
~/Documents/GeekMagic/weather-clock/
├── main.py                  ← 전체 진입점 (날씨+시계 서비스)
├── config.json              ← 설정 파일
├── requirements.txt
├── install.sh               ← systemd 서비스 자동 등록
├── test_display.py          ← 화면 표시 단위 테스트
├── cpu_monitor.py           ← CPU/메모리 모니터 (독립 실행)
├── venv/                    ← Python 가상환경
├── src/
│   ├── weather_api.py       ← OpenWeatherMap 연동
│   ├── image_generator.py   ← Pillow 이미지 생성
│   ├── push_client.py       ← GeekMagic HTTP 클라이언트
│   ├── scheduler.py         ← 스케줄러
│   └── web_config.py        ← Flask 설정 서버
├── templates/
│   └── index.html           ← 웹 설정 UI
├── cache/                   ← 날씨 캐시, 이미지 캐시
└── logs/                    ← 실행 로그
```

---

## 7. 핵심 기술 정리

### ESP8266 HTTP API 엔드포인트

| 엔드포인트 | 메서드 | 설명 |
|-----------|--------|------|
| `/doUpload?dir=/image/` | POST | 이미지 업로드 (multipart/form-data) |
| `/set?img=/image//파일명` | GET | 표시 이미지 지정 |
| `/set?theme=N` | GET | 테마 전환 |
| `/set?theme=N&img=/image//파일명` | GET | 테마 전환 + 이미지 지정 동시 처리 (깜박임 방지) |
| `/set?brt=N` | GET | 밝기 조절 |
| `/app.json` | GET | 현재 테마 확인 |
| `/img.json` | GET | 현재 표시 이미지 경로 |
| `/v.json` | GET | 모델/버전 확인 |
| `/update` | GET/POST | OTA 펌웨어 업데이트 |

### 테마 번호

| 번호 | 이름 | 설명 |
|------|------|------|
| 1 | Weather Clock Today | 날씨+시계 (기본) |
| 2 | Weather Forecast | 날씨 예보 |
| 3 | Photo Album | **외부 이미지 표시 (필수)** |
| 4 | Time Style 1 | 시간 스타일 |
| 5 | Time Style 2 | 시간 스타일 |
| 6 | Time Style 3 | 시간 스타일 |
| 7 | Simple Weather Clock | 심플 날씨 시계 |

### 주요 주의사항

1. **Content-Length 중복 헤더 버그**: `requests` 사용 불가 → `http.client` 직접 사용
2. **외부 이미지 표시 조건**: 반드시 `theme=3` (Photo Album) 상태여야 함
3. **실제 파일명 고정**: `weather_clock.jpg` — 다른 파일명으로 업로드해도 무시됨
4. **img.json 자동 리셋**: 장치가 내부 저장 경로로 주기 리셋 → 파일 내용 자체를 덮어써야 함
5. **테마 전환 깜박임 방지**: `/set?theme=3&img=...` 복합 쿼리로 단일 요청 처리
6. **내장 테마 복원 시 이미지 불필요**: `theme=1`~`2`, `4`~`7`은 장치 자체 렌더링, 이미지 조작 무관
7. **venv 필수**: Python 3.13 + Raspberry Pi OS Bookworm에서 시스템 pip 차단 (PEP 668)

---

## 8. 현재 상태 및 향후 과제

| 항목 | 상태 | 비고 |
|------|------|------|
| 장치 HTTP API 파악 | ✅ 완료 | 모든 엔드포인트 확인, 복합 쿼리 지원 확인 |
| venv 환경 구성 | ✅ 완료 | |
| requests 버그 우회 | ✅ 완료 | http.client 직접 사용 |
| weather_clock.jpg 파일명 발견 | ✅ 완료 | 핵심 돌파구 |
| cpu_monitor.py 테마 전환 깜박임 | ✅ 완료 | 복합 쿼리(`theme=3&img=...`) 단일 요청으로 해결 |
| main.py 날씨+시계 서비스 | ⏸ 대기 | OpenWeatherMap API Key 발급 후 테스트 필요 |
| install.sh systemd 등록 | ⏸ 대기 | 경로 문제 수정 완료, 재시도 필요 |
| OpenWeatherMap API Key | ⏸ 대기 | 미발급 |
| 웹 설정 UI (Flask) | ✅ 코드 완성 | 서비스 등록 후 테스트 필요 |

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

*최종 업데이트: 2026-05-08*
