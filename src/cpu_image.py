# src/cpu_image.py
# 작은 GeekMagic 화면용 심플 CPU 모니터 (240×240)

from PIL import Image, ImageDraw, ImageFont, ImageFilter
import os
import psutil

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


def _color(value):
    if value < 55:
        return CYAN
    if value < 80:
        return YELLOW
    return PINK


def _glow_text(img, xy, text, font, color, radius=5, anchor=None):
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.text(xy, text, font=font, fill=(*color, 220), anchor=anchor)
    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(radius)))
    ImageDraw.Draw(img).text(xy, text, font=font, fill=(*color, 255), anchor=anchor)


def generate_cpu_image() -> Image.Image:
    cpu = psutil.cpu_percent(interval=1)
    mem = psutil.virtual_memory().percent
    disk = psutil.disk_usage("/").percent
    try:
        with open("/sys/class/thermal/thermal_zone0/temp") as f:
            temp = int(f.read()) / 1000
    except Exception:
        temp = 0.0

    img = Image.new("RGBA", (SIZE, SIZE), (*BLACK, 255))
    draw = ImageDraw.Draw(img)
    draw.rounded_rectangle((9, 9, 230, 230), radius=15, outline=(*PINK, 180), width=2)

    draw.text((23, 14), "PI 5 MONITOR", font=_font(17), fill=(*WHITE, 255))
    draw.ellipse((213, 18, 222, 27), fill=(*CYAN, 255))
    draw.line((15, 42, 225, 42), fill=(*CYAN, 170), width=2)

    draw.text((18, 58), "CPU", font=_font(22), fill=(*CYAN, 255))
    _glow_text(img, (220, 51), f"{cpu:.0f}%", _font(58), _color(cpu), 6, "ra")

    draw.rounded_rectangle((14, 121, 226, 158), radius=10, fill=(*PANEL, 255), outline=(*YELLOW, 150))
    draw.text((25, 128), "TEMP", font=_font(16), fill=(*MUTED, 255))
    draw.text((214, 123), f"{temp:.1f}°C", font=_font(30), fill=(*YELLOW, 255), anchor="ra")

    draw.rounded_rectangle((14, 171, 111, 222), radius=10, fill=(*PANEL, 255), outline=(*CYAN, 150))
    draw.rounded_rectangle((129, 171, 226, 222), radius=10, fill=(*PANEL, 255), outline=(*VIOLET, 150))
    draw.text((25, 178), "MEM", font=_font(14), fill=(*WHITE, 255))
    draw.text((101, 199), f"{mem:.0f}%", font=_font(27), fill=(*CYAN, 255), anchor="rm")
    draw.text((140, 178), "DISK", font=_font(14), fill=(*WHITE, 255))
    draw.text((216, 199), f"{disk:.0f}%", font=_font(27), fill=(*VIOLET, 255), anchor="rm")
    return img.convert("RGB")
