# src/image_generator.py
# Pillow 기반 240×240 날씨+시계 이미지 생성 엔진

from PIL import Image, ImageDraw, ImageFont
import datetime
import pytz
import os
import logging

logger = logging.getLogger(__name__)

KST = pytz.timezone("Asia/Seoul")

# ── 색상 팔레트 ──────────────────────────────────────────────
C = {
    # 배경 그라디언트
    "bg_top":      (12, 18, 42),
    "bg_bottom":   (25, 35, 80),
    # 헤더 바
    "header_bg":   (0, 80, 160),
    "header_text": (255, 255, 255),
    # 시간
    "time_main":   (255, 220, 50),
    "time_sub":    (180, 190, 220),
    # 날씨
    "temp_main":   (255, 130, 80),
    "temp_feels":  (180, 180, 200),
    "weather_desc":(140, 210, 255),
    # 습도/풍속
    "info_label":  (120, 130, 160),
    "info_value":  (200, 210, 240),
    # 구분선
    "divider":     (40, 55, 100),
    # 푸터
    "footer_bg":   (15, 22, 55),
    "footer_text": (100, 110, 160),
    # 날씨 아이콘 배경
    "icon_bg":     (20, 40, 90),
}

# ── 폰트 경로 ─────────────────────────────────────────────────
ASSETS_DIR = os.path.join(os.path.dirname(os.path.dirname(__file__)), "assets", "fonts")
SYSTEM_FONTS = [
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",
    "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf",
]


def _get_font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    """시스템 폰트 자동 탐색 후 로드"""
    for path in SYSTEM_FONTS:
        if os.path.exists(path):
            try:
                return ImageFont.truetype(path, size)
            except Exception:
                continue
    # 폴백: PIL 기본 폰트
    return ImageFont.load_default()


def _draw_bg(draw: ImageDraw.Draw, width=240, height=240):
    """세로 그라디언트 배경"""
    r0, g0, b0 = C["bg_top"]
    r1, g1, b1 = C["bg_bottom"]
    for y in range(height):
        t = y / height
        r = int(r0 + (r1 - r0) * t)
        g = int(g0 + (g1 - g0) * t)
        b = int(b0 + (b1 - b0) * t)
        draw.line([(0, y), (width - 1, y)], fill=(r, g, b))


def _centered_text(draw, text, y, font, color, width=240):
    """텍스트 중앙 정렬"""
    bbox = draw.textbbox((0, 0), text, font=font)
    w = bbox[2] - bbox[0]
    x = (width - w) // 2
    draw.text((x, y), text, fill=color, font=font)
    return bbox[3] - bbox[1]  # 텍스트 높이 반환


def generate_weather_clock(weather: dict, config: dict) -> Image.Image:
    """
    날씨 + 시계 화면 생성
    Returns: PIL Image (240×240 RGB)
    """
    img = Image.new("RGB", (240, 240))
    draw = ImageDraw.Draw(img)

    # 배경
    _draw_bg(draw)

    now = datetime.datetime.now(KST)
    time_fmt = config.get("time_format", "24h")

    # ── 폰트 로드 ──────────────────────────────────────────
    font_city   = _get_font(13)
    font_time   = _get_font(52, bold=True)
    font_date   = _get_font(14)
    font_temp   = _get_font(28, bold=True)
    font_info   = _get_font(13)
    font_small  = _get_font(11)
    font_icon   = _get_font(30)

    # ── 헤더: 도시명 + 날씨 아이콘 ─────────────────────────
    # 헤더 배경 (라운드 없이 심플)
    draw.rectangle([0, 0, 239, 36], fill=C["header_bg"])

    city_name = weather.get("city", config.get("city", "Seoul"))
    draw.text((10, 10), city_name, fill=C["header_text"], font=font_city)

    # 날씨 아이콘 + 설명 (헤더 우측)
    icon = weather.get("icon", "🌡")
    desc = weather.get("desc_ko", "")
    draw.text((155, 10), f"{icon} {desc}", fill=(220, 235, 255), font=font_small)

    # ── 시간 표시 ───────────────────────────────────────────
    if time_fmt == "12h":
        time_str = now.strftime("%I:%M")
        ampm = now.strftime("%p")
    else:
        time_str = now.strftime("%H:%M")
        ampm = None

    # 시간 그림자 효과
    draw.text((61, 46), time_str, fill=(0, 0, 0, 80), font=font_time)
    draw.text((60, 45), time_str, fill=C["time_main"], font=font_time)

    if ampm:
        draw.text((185, 55), ampm, fill=C["time_sub"], font=font_info)

    # ── 날짜 ────────────────────────────────────────────────
    WEEKDAYS = ["월", "화", "수", "목", "금", "토", "일"]
    weekday = WEEKDAYS[now.weekday()]
    date_str = now.strftime(f"%Y.%m.%d ({weekday})")
    _centered_text(draw, date_str, 100, font_date, C["time_sub"])

    # ── 구분선 ──────────────────────────────────────────────
    draw.line([(20, 120), (220, 120)], fill=C["divider"], width=1)

    # ── 온도 ────────────────────────────────────────────────
    temp = weather.get("temp", "--")
    feels = weather.get("feels_like", "--")
    unit_sym = "°C" if config.get("temp_unit", "metric") == "metric" else "°F"

    temp_str = f"{temp}{unit_sym}"
    # 온도 중앙 배치
    _centered_text(draw, temp_str, 126, font_temp, C["temp_main"])

    # 체감온도
    feels_str = f"체감 {feels}{unit_sym}"
    _centered_text(draw, feels_str, 158, font_small, C["temp_feels"])

    # ── 구분선 ──────────────────────────────────────────────
    draw.line([(20, 175), (220, 175)], fill=C["divider"], width=1)

    # ── 습도 / 풍속 / 구름 ──────────────────────────────────
    humidity = weather.get("humidity", "--")
    wind = weather.get("wind_speed", "--")
    clouds = weather.get("clouds", "--")

    # 3열 레이아웃
    col_w = 80
    items = [
        ("💧", f"{humidity}%",   "습도"),
        ("💨", f"{wind}m/s",    "풍속"),
        ("☁️", f"{clouds}%",    "구름"),
    ]
    for i, (icon_s, val, label) in enumerate(items):
        x = 10 + i * col_w
        draw.text((x + 2,  179), icon_s, fill=C["info_label"], font=font_small)
        draw.text((x + 22, 179), val,    fill=C["info_value"], font=font_info)
        draw.text((x + 22, 193), label,  fill=C["info_label"], font=font_small)

    # ── 푸터: 마지막 업데이트 시각 ──────────────────────────
    draw.rectangle([0, 213, 239, 239], fill=C["footer_bg"])
    updated = now.strftime("Updated %H:%M")
    _centered_text(draw, updated, 221, font_small, C["footer_text"])

    return img


def generate_loading_screen(message: str = "연결 중...") -> Image.Image:
    """WiFi 연결 대기 / 로딩 화면"""
    img = Image.new("RGB", (240, 240))
    draw = ImageDraw.Draw(img)
    _draw_bg(draw)

    font_big   = _get_font(18, bold=True)
    font_small = _get_font(13)

    _centered_text(draw, "🌤", 70, _get_font(48), (200, 220, 255))
    _centered_text(draw, "Weather Clock", 130, font_big, (200, 210, 240))
    _centered_text(draw, message, 158, font_small, (120, 130, 160))

    now = datetime.datetime.now(KST)
    _centered_text(draw, now.strftime("%H:%M:%S"), 190, font_small, (80, 90, 130))

    return img


def generate_error_screen(error_msg: str = "오류 발생") -> Image.Image:
    """에러 화면"""
    img = Image.new("RGB", (240, 240), color=(40, 10, 10))
    draw = ImageDraw.Draw(img)

    font_big   = _get_font(16, bold=True)
    font_small = _get_font(12)

    _centered_text(draw, "⚠️", 60, _get_font(40), (255, 80, 80))
    _centered_text(draw, "오류", 110, font_big, (255, 100, 100))

    # 에러 메시지 (긴 경우 줄바꿈)
    words = error_msg[:40]
    _centered_text(draw, words, 140, font_small, (200, 150, 150))

    now = datetime.datetime.now(KST)
    _centered_text(draw, now.strftime("%H:%M"), 200, font_small, (120, 80, 80))

    return img


def save_image(img: Image.Image, path: str, quality: int = 88) -> bool:
    """이미지를 JPEG로 저장"""
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        img.save(path, "JPEG", quality=quality, optimize=True)
        logger.debug(f"이미지 저장: {path}")
        return True
    except Exception as e:
        logger.error(f"이미지 저장 실패: {e}")
        return False
