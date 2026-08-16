"""
SmallTV Ultra — Drawing Command Client

Pi 쪽에서 ESP8266에 드로잉 커맨드를 전송하는 헬퍼 모듈.

사용 예:
    from draw_client import SmallTV, text, hline, rect, circle

    tv = SmallTV("192.168.219.122")
    tv.draw([
        text("center", 100, "Hello!", size=3, color="cyan"),
        hline(20, 130, 200, color="grey"),
    ], timeout=10)
"""

import requests

# ── 엘리먼트 헬퍼 함수 ────────────────────────────────────────────────────────

def text(x_or_align, y, content, *, size=1, color="white"):
    """텍스트 엘리먼트.

    x_or_align: 정수(x 좌표) 또는 "center" / "right"
    """
    if x_or_align in ("center", "right"):
        return {"type": "text", "y": y, "text": content,
                "size": size, "color": color, "align": x_or_align}
    return {"type": "text", "x": x_or_align, "y": y,
            "text": content, "size": size, "color": color}


def rect(x, y, w, h, color="white", *, fill=True):
    """사각형 엘리먼트. fill=False 이면 외곽선만."""
    return {"type": "rect", "x": x, "y": y, "w": w, "h": h,
            "color": color, "fill": fill}


def line(x1, y1, x2, y2, color="white"):
    """직선 엘리먼트."""
    return {"type": "line", "x1": x1, "y1": y1, "x2": x2, "y2": y2, "color": color}


def hline(x, y, length, color="white"):
    """수평선 엘리먼트."""
    return {"type": "hline", "x": x, "y": y, "len": length, "color": color}


def vline(x, y, length, color="white"):
    """수직선 엘리먼트."""
    return {"type": "vline", "x": x, "y": y, "len": length, "color": color}


def circle(x, y, r, color="white", *, fill=False):
    """원 엘리먼트. fill=True 이면 채운 원."""
    return {"type": "circle", "x": x, "y": y, "r": r, "color": color, "fill": fill}


# ── SmallTV 클라이언트 ────────────────────────────────────────────────────────

class SmallTV:
    """SmallTV Ultra HTTP 클라이언트."""

    def __init__(self, host: str, timeout: int = 5):
        """
        host: 장치 IP 주소 (예: "192.168.219.122")
        timeout: HTTP 요청 타임아웃 (초)
        """
        self.base = f"http://{host}"
        self.timeout = timeout

    # ── 드로잉 커맨드 ──────────────────────────────────────────────────────────

    def draw(self, elements: list, *,
             clear: bool = True,
             bg: str = "black",
             timeout: int = 0) -> bool:
        """드로잉 커맨드를 전송합니다.

        elements: text(), rect(), line() 등으로 만든 엘리먼트 리스트
        clear:    전송 전 화면 지우기 (기본 True)
        bg:       배경색 (clear=True 일 때)
        timeout:  N초 후 자동으로 시계 모드로 복귀 (0 = 유지)
        """
        payload = {"clear": clear, "bg": bg, "elements": elements}
        if timeout:
            payload["timeout"] = timeout
        r = requests.post(f"{self.base}/draw", json=payload, timeout=self.timeout)
        r.raise_for_status()
        return True

    def clear(self, bg: str = "black") -> bool:
        """화면을 지웁니다."""
        return self.draw([], bg=bg)

    # ── 모드 제어 ─────────────────────────────────────────────────────────────

    def clock(self) -> bool:
        """시계 모드로 복귀합니다."""
        r = requests.get(f"{self.base}/mode?set=clock", timeout=self.timeout)
        r.raise_for_status()
        return True

    def status(self) -> dict:
        """장치 상태를 반환합니다. {"fw":..., "mode":..., "brt":..., "heap":...}"""
        r = requests.get(f"{self.base}/", timeout=self.timeout)
        r.raise_for_status()
        return r.json()

    # ── 밝기 ──────────────────────────────────────────────────────────────────

    def brightness(self, level: int) -> bool:
        """백라이트 밝기 설정 (0–255)."""
        r = requests.get(f"{self.base}/set?brt={level}", timeout=self.timeout)
        r.raise_for_status()
        return True


# ── 예제 ──────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    import time

    DEVICE_IP = "192.168.219.122"
    tv = SmallTV(DEVICE_IP)

    print("상태 확인:", tv.status())

    print("커스텀 화면 표시 (10초 후 시계 자동 복귀)...")
    tv.draw([
        text("center", 88,  "SmallTV Ultra",    size=2, color="cyan"),
        hline(20, 116, 200,                              color="grey"),
        text("center", 126, "Drawing Commands", size=1, color="white"),
        text("center", 144, "Phase 1 Active",   size=1, color="green"),
        circle(120, 185, 18,                             color="yellow", fill=True),
    ], timeout=10)

    print("10초 대기 중... (장치는 자동으로 시계로 복귀)")
    time.sleep(11)
    print("완료.")
