# src/cpu_image.py
# GeekMagic 240×240 네온 애니메이션 HUD 스타일 시스템 모니터

from PIL import Image, ImageDraw, ImageFont, ImageFilter
import datetime
import os
import time
import psutil
import pytz

KST = pytz.timezone("Asia/Seoul")
SIZE = 240
BLACK = (2, 3, 8)
PANEL = (8, 11, 22)
CYAN = (38, 245, 255)
PINK = (255, 61, 177)
VIOLET = (145, 89, 255)
YELLOW = (255, 226, 85)
WHITE = (240, 247, 255)
MUTED = (112, 137, 166)


def _get_font(size: int) -> ImageFont.FreeTypeFont:
    for path in [
        "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",
    ]:
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def _color(value):
    if value < 55:
        return CYAN
    if value < 80:
        return YELLOW
    return PINK


def _glow_arc(img, box, start, end, color, width=5):
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    ld.arc(box, start, end, fill=(*color, 220), width=width + 3)
    img.alpha_composite(layer.filter(ImageFilter.GaussianBlur(6)))
    ImageDraw.Draw(img).arc(box, start, end, fill=(*color, 255), width=width)


def _glow_text(img, xy, text, font, color, radius=5, anchor=None):
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    ld.text(xy, text, font=font, fill=(*color, 220), anchor=anchor)
    img.alpha_composite(layer.filter(ImageFilter.GaussianBlur(radius)))
    ImageDraw.Draw(img).text(xy, text, font=font, fill=(*color, 255), anchor=anchor)


def _draw_background(img):
    draw = ImageDraw.Draw(img)
    for y in range(SIZE):
        t = y / SIZE
        draw.line((0, y, SIZE, y), fill=(2, int(3 + 5 * t), int(8 + 11 * t), 255))
    # HUD 그리드와 속도선
    for x in range(0, SIZE, 24):
        draw.line((x, 0, x, SIZE), fill=(20, 34, 55, 55))
    for y in range(0, SIZE, 24):
        draw.line((0, y, SIZE, y), fill=(20, 34, 55, 45))
    draw.line((-10, 220, 102, 165), fill=(*CYAN, 80), width=1)
    draw.line((142, 31, 250, -4), fill=(*PINK, 90), width=1)
    for x, y, c in [(12, 54, CYAN), (225, 73, PINK), (20, 190, VIOLET), (207, 213, CYAN)]:
        draw.line((x - 3, y, x + 3, y), fill=(*c, 170))
        draw.line((x, y - 3, x, y + 3), fill=(*c, 170))


def _panel(draw, box, color=VIOLET, radius=10):
    draw.rounded_rectangle(box, radius=radius, fill=(*PANEL, 238), outline=(*color, 160), width=1)


def _mini_bar(draw, x, y, width, value, color):
    draw.rounded_rectangle((x, y, x + width, y + 5), radius=2, fill=(19, 28, 43, 255))
    fill_w = max(2, int(width * min(max(value, 0), 100) / 100))
    draw.rounded_rectangle((x, y, x + fill_w, y + 5), radius=2, fill=(*color, 255))
    draw.ellipse((x + fill_w - 2, y - 1, x + fill_w + 3, y + 6), fill=(*WHITE, 230))


def generate_cpu_image() -> Image.Image:
    cpu = psutil.cpu_percent(interval=1)
    mem = psutil.virtual_memory()
    disk = psutil.disk_usage("/")
    try:
        with open("/sys/class/thermal/thermal_zone0/temp") as f:
            cpu_temp = int(f.read()) / 1000
    except Exception:
        cpu_temp = 0.0

    now = datetime.datetime.now(KST)
    img = Image.new("RGBA", (SIZE, SIZE), (*BLACK, 255))
    _draw_background(img)
    draw = ImageDraw.Draw(img)

    f_title = _get_font(11)
    f_tiny = _get_font(8)
    f_label = _get_font(9)
    f_value = _get_font(15)
    f_cpu = _get_font(28)

    # 헤더
    draw.rounded_rectangle((8, 8, 232, 32), radius=11, fill=(7, 13, 25, 245),
                           outline=(*CYAN, 170), width=1)
    draw.ellipse((16, 16, 22, 22), fill=(*PINK, 255))
    draw.text((28, 13), "PI5 // SYSTEM CORE", font=f_title, fill=(*WHITE, 255))
    draw.text((181, 16), now.strftime("%H:%M:%S"), font=f_tiny, fill=(*CYAN, 255))

    # CPU 네온 링
    ring_box = (13, 47, 119, 153)
    draw.ellipse(ring_box, outline=(24, 38, 60, 255), width=7)
    draw.arc(ring_box, -90, 270, fill=(38, 48, 75, 255), width=5)
    cpu_end = -90 + 360 * min(cpu, 100) / 100
    _glow_arc(img, ring_box, -90, cpu_end, _color(cpu), 5)
    _glow_text(img, (66, 88), f"{cpu:.0f}%", f_cpu, _color(cpu), 6, "mm")
    draw.text((66, 116), "CPU LOAD", font=f_label, fill=(*MUTED, 255), anchor="mm")
    draw.polygon([(104, 49), (115, 49), (115, 60)], fill=(*PINK, 190))

    # 우측 정보 카드
    cards = [
        ("TEMP", f"{cpu_temp:.1f}°C", min(cpu_temp, 100), YELLOW),
        ("MEMORY", f"{mem.percent:.0f}%", mem.percent, CYAN),
        ("DISK", f"{disk.percent:.0f}%", disk.percent, VIOLET),
    ]
    for i, (label, value, pct, color) in enumerate(cards):
        y = 47 + i * 38
        _panel(draw, (128, y, 232, y + 32), color, 8)
        draw.text((136, y + 6), label, font=f_tiny, fill=(*MUTED, 255))
        draw.text((224, y + 4), value, font=f_value, fill=(*color, 255), anchor="ra")
        _mini_bar(draw, 136, y + 22, 87, pct, color)

    # 하단 메모리 상세 + 업타임
    _panel(draw, (8, 165, 232, 226), PINK, 12)
    mem_used = mem.used / (1024 ** 3)
    mem_total = mem.total / (1024 ** 3)
    disk_used = disk.used / (1024 ** 3)
    disk_total = disk.total / (1024 ** 3)
    draw.text((18, 174), "MEM", font=f_label, fill=(*CYAN, 255))
    draw.text((57, 173), f"{mem_used:.1f}/{mem_total:.1f}G", font=_get_font(11), fill=(*WHITE, 255))
    draw.text((18, 194), "SSD", font=f_label, fill=(*VIOLET, 255))
    draw.text((57, 193), f"{disk_used:.0f}/{disk_total:.0f}G", font=_get_font(11), fill=(*WHITE, 255))

    uptime = int(time.time() - psutil.boot_time())
    days, rem = divmod(uptime, 86400)
    hours = rem // 3600
    draw.rounded_rectangle((151, 171, 222, 217), radius=8, fill=(13, 8, 28, 255),
                           outline=(*PINK, 150), width=1)
    draw.text((187, 179), "UPTIME", font=f_tiny, fill=(*MUTED, 255), anchor="mm")
    _glow_text(img, (187, 201), f"{days}D {hours}H", f_value, PINK, 4, "mm")

    draw.text((10, 231), "NEON CORE // ONLINE", font=_get_font(7), fill=(*MUTED, 190))
    draw.text((196, 231), now.strftime("%m.%d"), font=_get_font(7), fill=(*CYAN, 210))
    return img.convert("RGB")
