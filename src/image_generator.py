# src/image_generator.py
# GeekMagic 240×240 네온 애니메이션 스타일 날씨 + 시계 렌더러

from PIL import Image, ImageDraw, ImageFont, ImageFilter
import datetime
import pytz
import os
import logging

logger = logging.getLogger(__name__)
KST = pytz.timezone("Asia/Seoul")
SIZE = 240

C = {
    "black": (2, 3, 8),
    "panel": (8, 11, 22),
    "cyan": (40, 245, 255),
    "pink": (255, 62, 176),
    "violet": (185, 120, 255),
    "yellow": (255, 226, 88),
    "white": (244, 249, 255),
    "muted": (126, 150, 178),
    "blue": (54, 128, 255),
}

SYSTEM_FONTS = [
    "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",
]


def _get_font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    for path in SYSTEM_FONTS:
        if os.path.exists(path):
            try:
                return ImageFont.truetype(path, size)
            except Exception:
                pass
    return ImageFont.load_default()


def _text_center(draw, text, y, font, fill, width=SIZE):
    box = draw.textbbox((0, 0), text, font=font)
    x = (width - (box[2] - box[0])) // 2
    draw.text((x, y), text, font=font, fill=fill)


def _glow_text(img, xy, text, font, color, radius=5, anchor=None):
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.text(xy, text, font=font, fill=(*color, 210), anchor=anchor)
    blurred = glow.filter(ImageFilter.GaussianBlur(radius))
    img.alpha_composite(blurred)
    ImageDraw.Draw(img).text(xy, text, font=font, fill=(*color, 255), anchor=anchor)


def _glow_line(img, points, color, width=1, radius=4):
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.line(points, fill=(*color, 190), width=max(width + 2, 3))
    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(radius)))
    ImageDraw.Draw(img).line(points, fill=(*color, 255), width=width)


def _background(img):
    draw = ImageDraw.Draw(img)
    for y in range(SIZE):
        t = y / SIZE
        draw.line((0, y, SIZE, y), fill=(2, int(3 + 4 * t), int(8 + 10 * t), 255))

    # 애니메이션 오프닝을 연상시키는 네온 궤적
    _glow_line(img, [(-15, 53), (62, 20), (146, 28), (255, -5)], C["violet"], 1, 6)
    _glow_line(img, [(-12, 232), (52, 205), (146, 223), (255, 178)], C["cyan"], 1, 5)
    _glow_line(img, [(155, 0), (187, 25), (224, 30), (250, 55)], C["pink"], 1, 5)

    # 별빛/스파클 — 고정 좌표라 화면이 흔들리지 않음
    stars = [(9, 45), (31, 90), (218, 67), (205, 101), (20, 170), (231, 159),
             (72, 9), (151, 13), (97, 181), (183, 193), (12, 219)]
    for i, (x, y) in enumerate(stars):
        color = C["cyan"] if i % 3 == 0 else C["pink"] if i % 3 == 1 else C["white"]
        draw.point((x, y), fill=(*color, 210))
        if i % 4 == 0:
            draw.line((x - 2, y, x + 2, y), fill=(*color, 150))
            draw.line((x, y - 2, x, y + 2), fill=(*color, 150))


def _panel(draw, box, outline=C["violet"], radius=12, fill=(7, 10, 20, 235)):
    draw.rounded_rectangle(box, radius=radius, fill=fill, outline=(*outline, 170), width=1)


def _weather_icon(img, center, code):
    """이모지 폰트에 의존하지 않는 애니메이션 셀풍 날씨 아이콘."""
    cx, cy = center
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)

    is_night = str(code).endswith("n")
    kind = str(code)[:2]
    if kind == "01":
        color = C["violet"] if is_night else C["yellow"]
        gd.ellipse((cx - 13, cy - 13, cx + 13, cy + 13), fill=(*color, 220))
        for a, b, c, d in [(-23, 0, -17, 0), (17, 0, 23, 0), (0, -23, 0, -17),
                           (0, 17, 0, 23), (-17, -17, -13, -13), (13, 13, 17, 17),
                           (-17, 17, -13, 13), (13, -13, 17, -17)]:
            gd.line((cx + a, cy + b, cx + c, cy + d), fill=(*color, 230), width=2)
    else:
        # 구름
        gd.ellipse((cx - 18, cy - 7, cx + 3, cy + 12), fill=(*C["white"], 230))
        gd.ellipse((cx - 7, cy - 15, cx + 14, cy + 12), fill=(*C["white"], 230))
        gd.rounded_rectangle((cx - 20, cy, cx + 20, cy + 14), radius=7, fill=(*C["white"], 230))
        if kind in {"09", "10", "11"}:
            for dx in (-12, 0, 12):
                gd.line((cx + dx, cy + 18, cx + dx - 3, cy + 25), fill=(*C["cyan"], 255), width=2)
        elif kind == "13":
            for dx in (-10, 8):
                gd.text((cx + dx, cy + 14), "*", font=_get_font(12), fill=(*C["cyan"], 255))

    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(7)))
    img.alpha_composite(glow)


def generate_weather_clock(weather: dict, config: dict) -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (*C["black"], 255))
    _background(img)
    draw = ImageDraw.Draw(img)
    now = datetime.datetime.now(KST)

    f_city = _get_font(11, True)
    f_tiny = _get_font(9)
    f_time = _get_font(48, True)
    f_temp = _get_font(34, True)
    f_desc = _get_font(11, True)
    f_stat = _get_font(12, True)

    # 상단 캡슐
    draw.rounded_rectangle((9, 8, 146, 31), radius=11, fill=(7, 14, 27, 235),
                           outline=(*C["cyan"], 180), width=1)
    draw.ellipse((16, 15, 22, 21), fill=(*C["pink"], 255))
    city = str(weather.get("city", config.get("city", "SEOUL"))).upper()[:17]
    draw.text((28, 13), city, font=f_city, fill=(*C["white"], 255))
    draw.text((157, 13), now.strftime("%m.%d  %a").upper(), font=f_tiny, fill=(*C["muted"], 255))

    # 메인 시계
    time_str = now.strftime("%I:%M" if config.get("time_format") == "12h" else "%H:%M")
    _glow_text(img, (120, 61), time_str, f_time, C["cyan"], 7, "mm")
    if config.get("time_format") == "12h":
        draw.text((200, 70), now.strftime("%p"), font=f_tiny, fill=(*C["pink"], 255))
    draw.rounded_rectangle((84, 89, 156, 93), radius=2, fill=(*C["pink"], 180))

    # 날씨 카드
    _panel(draw, (8, 101, 232, 176), C["violet"], 14)
    draw.rounded_rectangle((17, 110, 72, 167), radius=14, fill=(14, 14, 34, 255),
                           outline=(*C["pink"], 170), width=1)
    _weather_icon(img, (45, 137), weather.get("icon_code", "01d"))

    temp = weather.get("temp", "--")
    unit = "°C" if config.get("temp_unit", "metric") == "metric" else "°F"
    _glow_text(img, (84, 108), f"{temp}{unit}", f_temp, C["yellow"], 5)
    # 기본 설치 폰트에는 한글 글리프가 없어 영문 설명을 사용한다.
    desc = str(weather.get("desc_en", "WEATHER")).upper()[:13]
    draw.text((86, 147), desc, font=f_desc, fill=(*C["pink"], 255))
    feels = weather.get("feels_like", "--")
    draw.text((171, 149), f"FEEL {feels}°", font=_get_font(10), fill=(*C["white"], 255))

    # 하단 3개 미니 카드
    stats = [
        ("HUM", f"{weather.get('humidity', '--')}%", C["cyan"]),
        ("WIND", f"{weather.get('wind_speed', '--')}", C["pink"]),
        ("CLOUD", f"{weather.get('clouds', '--')}%", C["violet"]),
    ]
    for i, (label, value, color) in enumerate(stats):
        x0 = 8 + i * 76
        _panel(draw, (x0, 183, x0 + 70, 228), color, 10, (6, 9, 18, 240))
        draw.text((x0 + 8, 190), label, font=f_tiny, fill=(*C["muted"], 255))
        draw.text((x0 + 8, 205), value, font=f_stat, fill=(*color, 255))
        draw.ellipse((x0 + 55, 191, x0 + 60, 196), fill=(*color, 255))

    draw.text((10, 231), "NEON WEATHER // LIVE", font=_get_font(7), fill=(*C["muted"], 180))
    draw.text((188, 231), now.strftime("%H:%M"), font=_get_font(7), fill=(*C["pink"], 210))
    return img.convert("RGB")


def generate_loading_screen(message: str = "CONNECTING...") -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (*C["black"], 255))
    _background(img)
    _glow_text(img, (120, 91), "◇", _get_font(55, True), C["cyan"], 8, "mm")
    _glow_text(img, (120, 139), "WEATHER LINK", _get_font(17, True), C["pink"], 5, "mm")
    ImageDraw.Draw(img).text((120, 169), message, font=_get_font(11), fill=(*C["muted"], 255), anchor="mm")
    return img.convert("RGB")


def generate_error_screen(error_msg: str = "SYSTEM ERROR") -> Image.Image:
    img = Image.new("RGBA", (SIZE, SIZE), (*C["black"], 255))
    _background(img)
    _glow_text(img, (120, 88), "!", _get_font(64, True), C["pink"], 9, "mm")
    _glow_text(img, (120, 139), "SIGNAL LOST", _get_font(18, True), C["pink"], 5, "mm")
    ImageDraw.Draw(img).text((120, 170), str(error_msg)[:28], font=_get_font(10), fill=(*C["muted"], 255), anchor="mm")
    return img.convert("RGB")


def save_image(img: Image.Image, path: str, quality: int = 92) -> bool:
    try:
        os.makedirs(os.path.dirname(path), exist_ok=True)
        img.save(path, "JPEG", quality=quality, optimize=True, subsampling=0)
        logger.debug("이미지 저장: %s", path)
        return True
    except Exception as e:
        logger.error("이미지 저장 실패: %s", e)
        return False
