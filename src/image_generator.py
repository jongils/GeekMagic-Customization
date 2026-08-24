# src/image_generator.py
# 작은 GeekMagic 화면용 심플 네온 날씨/시계 렌더러 (240×240)

from PIL import Image, ImageDraw, ImageFont, ImageFilter
import datetime
import logging
import os
import pytz

logger = logging.getLogger(__name__)
KST = pytz.timezone("Asia/Seoul")
SIZE = 240

BLACK = (1, 2, 6)
PANEL = (8, 13, 24)
WHITE = (244, 249, 255)
MUTED = (145, 166, 188)
CYAN = (40, 244, 255)
PINK = (255, 65, 177)
YELLOW = (255, 225, 70)
VIOLET = (188, 121, 255)

FONT_PATHS = [
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/nunito-sans/NunitoSans-VariableFont_YTLC,opsz,wdth,wght.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
]


def _font(size):
    for path in FONT_PATHS:
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def _glow_text(img, xy, text, font, color, radius=4, anchor=None):
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.text(xy, text, font=font, fill=(*color, 220), anchor=anchor)
    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(radius)))
    ImageDraw.Draw(img).text(xy, text, font=font, fill=(*color, 255), anchor=anchor)


def _weather_icon(img, center, code):
    """이모지 폰트 없이 크게 보이는 단순 날씨 아이콘."""
    cx, cy = center
    code = str(code or "01d")
    kind = code[:2]
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)

    if kind == "01":
        d.ellipse((cx - 18, cy - 18, cx + 18, cy + 18), fill=(*YELLOW, 255))
        for x1, y1, x2, y2 in [(-30, 0, -23, 0), (23, 0, 30, 0), (0, -30, 0, -23),
                               (0, 23, 0, 30), (-22, -22, -17, -17), (17, 17, 22, 22),
                               (-22, 22, -17, 17), (17, -17, 22, -22)]:
            d.line((cx + x1, cy + y1, cx + x2, cy + y2), fill=(*YELLOW, 255), width=3)
    else:
        d.ellipse((cx - 27, cy - 7, cx + 2, cy + 19), fill=(*WHITE, 255))
        d.ellipse((cx - 10, cy - 22, cx + 20, cy + 19), fill=(*WHITE, 255))
        d.rounded_rectangle((cx - 30, cy, cx + 30, cy + 22), radius=11, fill=(*WHITE, 255))
        if kind in {"09", "10", "11"}:
            for dx in (-18, 0, 18):
                d.line((cx + dx, cy + 28, cx + dx - 5, cy + 39), fill=(*CYAN, 255), width=4)
        elif kind == "13":
            for dx in (-14, 12):
                d.line((cx + dx - 5, cy + 31, cx + dx + 5, cy + 31), fill=(*CYAN, 255), width=2)
                d.line((cx + dx, cy + 26, cx + dx, cy + 36), fill=(*CYAN, 255), width=2)

    img.alpha_composite(layer.filter(ImageFilter.GaussianBlur(7)))
    img.alpha_composite(layer)


def generate_weather_clock(weather: dict, config: dict) -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (*BLACK, 255))
    draw = ImageDraw.Draw(img)
    now = datetime.datetime.now(KST)

    # 최소한의 네온 프레임
    draw.rounded_rectangle((9, 9, 230, 230), radius=15, outline=(*CYAN, 180), width=2)
    draw.line((18, 102, 222, 102), fill=(*PINK, 180), width=2)

    city = str(weather.get("city", config.get("city", "SEOUL"))).upper()[:18]
    draw.text((17, 14), city, font=_font(14), fill=(*WHITE, 255))
    draw.text((222, 14), now.strftime("%m/%d"), font=_font(13), fill=(*CYAN, 255), anchor="ra")

    fmt = "%I:%M" if config.get("time_format") == "12h" else "%H:%M"
    _glow_text(img, (120, 63), now.strftime(fmt), _font(62), CYAN, 6, "mm")

    # 날씨는 아이콘 + 온도만 크게
    _weather_icon(img, (53, 145), weather.get("icon_code", "01d"))
    unit = "°C" if config.get("temp_unit", "metric") == "metric" else "°F"
    _glow_text(img, (132, 123), f"{weather.get('temp', '--')}{unit}", _font(41), YELLOW, 5)
    desc = str(weather.get("desc_en", "WEATHER")).upper()[:12]
    draw.text((136, 166), desc, font=_font(15), fill=(*PINK, 255))

    # 하단 핵심 정보 두 개만 큰 글씨로 표시
    draw.rounded_rectangle((13, 190, 113, 226), radius=9, fill=(*PANEL, 255), outline=(*CYAN, 150))
    draw.rounded_rectangle((127, 190, 227, 226), radius=9, fill=(*PANEL, 255), outline=(*VIOLET, 150))
    draw.text((23, 196), "HUM", font=_font(14), fill=(*WHITE, 255))
    draw.text((103, 194), f"{weather.get('humidity', '--')}%", font=_font(24), fill=(*CYAN, 255), anchor="ra")
    draw.text((137, 196), "WIND", font=_font(14), fill=(*WHITE, 255))
    draw.text((218, 194), f"{weather.get('wind_speed', '--')}", font=_font(24), fill=(*VIOLET, 255), anchor="ra")
    return img.convert("RGB")


def generate_loading_screen(message="CONNECTING"):
    img = Image.new("RGBA", (SIZE, SIZE), (*BLACK, 255))
    _glow_text(img, (120, 93), "●", _font(72), CYAN, 8, "mm")
    _glow_text(img, (120, 156), message, _font(22), PINK, 5, "mm")
    return img.convert("RGB")


def generate_error_screen(error_msg="ERROR"):
    img = Image.new("RGBA", (SIZE, SIZE), (*BLACK, 255))
    _glow_text(img, (120, 92), "!", _font(80), PINK, 9, "mm")
    _glow_text(img, (120, 160), str(error_msg)[:16].upper(), _font(20), WHITE, 4, "mm")
    return img.convert("RGB")


def save_image(img, path, quality=93):
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        img.save(path, "JPEG", quality=quality, optimize=True, subsampling=0)
        return True
    except Exception as exc:
        logger.error("이미지 저장 실패: %s", exc)
        return False
