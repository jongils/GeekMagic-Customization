# GeekMagic Weather Clock — 기능 개발 계획

> **작성일**: 2026-05-10  
> **상태 표기**: ✅ 완료 | 🔄 진행 중 | ⏸ 대기

---

## Feature 1: 로컬 폴더 슬라이드쇼 ✅

### 개요
지정한 로컬 폴더에 있는 사진을 GeekMagic 화면에 반복 표시.  
CPU 모니터와 동일한 방식으로 웹 UI에서 켜고 끌 수 있으며, 내장 테마와 교대로 표시하거나 연속 재생 모드를 선택할 수 있음.

### 동작 흐름

```
[연속 재생 모드]  사진1(N초) → 사진2(N초) → ... → 처음으로 반복

[교대 표시 모드]  GeekMagic 테마(rest_sec) → 사진1(show_sec) →
                 GeekMagic 테마(rest_sec) → 사진2(show_sec) → 반복
```

### 구현 항목

#### 1-A. `src/slideshow.py` 신규 생성 ✅
- 지정 폴더에서 JPG/PNG/BMP 파일 목록 수집
- 이미지를 240×240으로 리사이즈 (중앙 크롭)
- 셔플 모드 지원 (랜덤 순서)
- 캐시 폴더(`cache/slideshow/`)에 처리된 이미지 저장

```python
# 핵심 함수
def load_images(folder: str, shuffle: bool = False) -> list[str]
def resize_for_display(image_path: str, output_path: str) -> str
```

#### 1-B. `config.json` 슬라이드쇼 섹션 추가 ✅

```json
"slideshow": {
  "enabled": false,
  "folder": "/home/pi5/Pictures",
  "show_sec": 10,
  "rest_sec": 0,
  "shuffle": false,
  "restore_theme": 1
}
```

| 항목 | 설명 | 기본값 |
|------|------|--------|
| `enabled` | 슬라이드쇼 활성화 | `false` |
| `folder` | 사진 폴더 경로 | `/home/pi5/Pictures` |
| `show_sec` | 사진 1장 표시 시간 (초) | `10` |
| `rest_sec` | 사진 사이 GeekMagic 테마 표시 시간 (0=연속 재생) | `0` |
| `shuffle` | 랜덤 순서 재생 | `false` |
| `restore_theme` | 사진 사이 복원할 내장 테마 번호 | `1` |

#### 1-C. `src/scheduler.py` — `_slideshow_loop` 스레드 추가 ✅
- CPU 모니터와 동일한 구조 (`threading.Lock`, `_push_lock` 공유)
- 폴더 내용을 매 순환마다 재스캔 (파일 추가/삭제 실시간 반영)
- 이미지가 없을 경우 대기 후 재시도
- CPU 모니터와 슬라이드쇼 동시 활성화 시 상호 충돌 방지 처리

```python
# 추가 플래그
self._showing_slideshow = False

def _slideshow_loop(self):
    while self._running:
        images = load_images(folder, shuffle)
        for img_path in images:
            # rest_sec > 0 이면 GeekMagic 테마 표시 후
            # 이미지 push → show_sec 대기
            ...
```

#### 1-D. `src/web_config.py` — 슬라이드쇼 설정 저장/로드 ✅

#### 1-E. `templates/index.html` — 슬라이드쇼 설정 카드 추가 ✅
- 활성화 토글
- 폴더 경로 입력
- 사진 표시 시간 / 테마 표시 시간 선택
- 셔플 토글
- 복원 테마 선택 (CPU 모니터와 동일)
- 현재 폴더의 이미지 수 표시

#### 1-F. 슬라이드쇼 전용 업로드 파일명 — 생략 ✅
- 슬라이드쇼 이미지는 `slideshow_current.jpg`로 업로드
- CPU 이미지(`weather_clock.jpg`)와 파일명 충돌 방지

### 구현 순서
1. `src/slideshow.py` 이미지 처리 모듈
2. config 구조 추가
3. scheduler 슬라이드쇼 스레드
4. web_config 저장/로드
5. index.html UI 카드
6. 통합 테스트

---

## Feature 2: OpenClaw 수신 사진 표시 ⏸

### 배경
OpenClaw(AI 코딩 어시스턴트)는 사용자가 보낸 이미지를 로컬 폴더에 저장함.

- **수신 폴더**: `~/.openclaw/media/inbound/`
- **저장 형식**: JPEG (`file_0---[UUID].jpg`)
- **활용 시나리오**: OpenClaw에 이미지를 보내면 자동으로 GeekMagic 화면에 표시

### 동작 흐름

```
사용자 → OpenClaw에 이미지 전송
           ↓
~/.openclaw/media/inbound/ 에 파일 저장
           ↓
watcher가 신규 파일 감지 (폴링 또는 inotify)
           ↓
240×240 리사이즈 후 GeekMagic Push
           ↓
show_sec 동안 표시 → 이전 화면으로 복귀
```

### 구현 방식 비교

| 방식 | 장점 | 단점 |
|------|------|------|
| **A. 폴링** (추천) | 추가 라이브러리 없음, 단순 | 최대 poll_interval 지연 |
| B. watchdog 라이브러리 | 이벤트 기반, 즉각 반응 | 패키지 설치 필요 |
| C. inotifywait | 시스템 레벨, 가장 빠름 | 별도 프로세스, 복잡 |

→ **A. 폴링 방식** 채택 (5초 간격, 실용적 지연 수준)

### 구현 항목

#### 2-A. `src/photo_watcher.py` 신규 생성 ⏸
- 지정 폴더를 N초마다 폴링
- 이미 표시한 파일은 `shown_files` 집합으로 추적 (재표시 방지)
- 신규 파일 감지 시 콜백 호출
- 이미지 240×240 리사이즈 + Push 수행

```python
class PhotoWatcher:
    def __init__(self, watch_folder, push_client, poll_sec=5):
        self.shown_files: set[str] = set()

    def _scan_new_files(self) -> list[str]:
        """신규 파일만 반환, shown_files에 추가"""

    def watch_loop(self):
        while self._running:
            new_files = self._scan_new_files()
            for f in new_files:
                self._display(f)  # 리사이즈 → Push → show_sec 대기
            time.sleep(self.poll_sec)
```

#### 2-B. `config.json` photo_watcher 섹션 추가 ⏸

```json
"photo_watcher": {
  "enabled": false,
  "watch_folder": "/home/pi5/.openclaw/media/inbound",
  "show_sec": 30,
  "poll_sec": 5,
  "restore_theme": 1
}
```

| 항목 | 설명 | 기본값 |
|------|------|--------|
| `enabled` | 자동 표시 활성화 | `false` |
| `watch_folder` | 감시할 폴더 경로 | `~/.openclaw/media/inbound` |
| `show_sec` | 신규 사진 표시 시간 (초) | `30` |
| `poll_sec` | 폴더 폴링 간격 (초) | `5` |
| `restore_theme` | 사진 종료 후 복원 테마 | `1` |

#### 2-C. `src/scheduler.py` — PhotoWatcher 스레드 연동 ⏸
- `start_background()`에서 `photo_watcher.enabled` 확인 후 스레드 시작
- 신규 사진 표시 중 CPU 모니터/슬라이드쇼 Push 차단

#### 2-D. `src/web_config.py` — photo_watcher 설정 저장/로드 ⏸

#### 2-E. `templates/index.html` — photo_watcher 설정 카드 추가 ⏸
- 활성화 토글
- 감시 폴더 경로 입력
- 표시 시간 / 폴링 간격 선택
- 마지막 수신 파일명 + 수신 시간 표시

### 미결 사항 (구현 전 확인 필요)

| 항목 | 내용 |
|------|------|
| 카메라 연결 여부 | `No cameras available` — RPi 카메라 미연결. 필요 시 `libcamera`/`picamera2` 연동 추가 고려 |
| OpenClaw 이미지 경로 고정 여부 | 현재 `~/.openclaw/media/inbound/` 확인. 버전 업 시 경로 변경 가능성 있음 |
| 기존 파일 처리 | 서비스 시작 시 이미 존재하는 파일은 표시 생략 (시작 시점 이후 신규 파일만 표시) |

### 구현 순서
1. `src/photo_watcher.py` 파일 감시 모듈
2. config 구조 추가
3. scheduler 연동
4. web_config 저장/로드
5. index.html UI 카드
6. 통합 테스트

---

## 전체 일정

```
Feature 1 (슬라이드쇼)
  ├── 1-A: slideshow.py 이미지 모듈        ✅
  ├── 1-B: config 구조                     ✅
  ├── 1-C: scheduler 스레드                ✅
  ├── 1-D: web_config 저장/로드            ✅
  └── 1-E: index.html UI                  ✅

Feature 2 (OpenClaw 사진)
  ├── 2-A: photo_watcher.py 감시 모듈      ⏸
  ├── 2-B: config 구조                     ⏸
  ├── 2-C: scheduler 연동                  ⏸
  ├── 2-D: web_config 저장/로드            ⏸
  └── 2-E: index.html UI                  ⏸
```

---

## 공통 고려사항

### 이미지 처리 파이프라인 (Feature 1 & 2 공통)
- 원본 해상도 무관하게 240×240으로 리사이즈 (비율 유지 후 중앙 크롭)
- JPEG quality=85 저장
- EXIF 방향 정보 자동 적용 (세로 사진 회전 처리)
- 지원 포맷: JPEG, PNG, BMP, WEBP

### Push 충돌 방지 우선순위
```
photo_watcher (즉각 표시, 최우선)
  > slideshow (예약 표시)
    > cpu_monitor (주기 표시)
      > weather (정기 push)
```

### 파일명 규칙 (장치 업로드)
| 기능 | 장치 업로드 파일명 |
|------|-----------------|
| 날씨+시계 | `weather_clock.jpg` |
| CPU 모니터 | `weather_clock.jpg` (현재) |
| 슬라이드쇼 | `weather_clock.jpg` (동일 파일 덮어쓰기) |
| Photo Watcher | `weather_clock.jpg` (동일) |

> GeekMagic 장치는 항상 `weather_clock.jpg` 파일명을 내부 참조하므로 모든 기능이 동일 파일명으로 업로드함.

---

*최종 업데이트: 2026-05-10*
