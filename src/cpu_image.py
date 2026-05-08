# src/cpu_image.py
# CPU/메모리/디스크 모니터 240×240 이미지 생성 (스케줄러 공용 모듈)

from PIL import Image, ImageDraw, ImageFont
import datetime
import os
import time
import psutil
import pytz

KST = pytz.timezone("Asia/Seoul")


def _get_font(size: int) -> ImageFont.FreeTypeFont:
    for path in [
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",
    ]:
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def _val_color(v: float) -> tuple:
    if v < 50: return (80, 220, 100)
    if v < 80: return (255, 200, 50)
    return (255, 80, 80)


def _bar_color(v: float) -> tuple:
    if v < 50: return (40, 160, 70)
    if v < 80: return (180, 140, 30)
    return (180, 50, 50)


def _draw_bar(draw: ImageDraw.Draw, x, y, w, h, val, color):
    draw.rectangle([x, y, x + w, y + h], fill=(30, 35, 60))
    fw = int(w * min(val, 100) / 100)
    if fw > 0:
        draw.rectangle([x, y, x + fw, y + h], fill=color)
    draw.rectangle([x, y, x + w, y + h], outline=(50, 60, 100), width=1)


def generate_cpu_image() -> Image.Image:
    """CPU/메모리/디스크 모니터 화면 생성 — PIL Image (240×240) 반환"""
    cpu       = psutil.cpu_percent(interval=1)
    mem       = psutil.virtual_memory()
    mem_pct   = mem.percent
    mem_used  = mem.used  / (1024 ** 3)
    mem_total = mem.total / (1024 ** 3)
    disk      = psutil.disk_usage("/")
    disk_pct  = disk.percent

    try:
        with open("/sys/class/thermal/thermal_zone0/temp") as f:
            cpu_temp = int(f.read()) / 1000
    except Exception:
        cpu_temp = 0.0

    now  = datetime.datetime.now(KST)
    img  = Image.new("RGB", (240, 240))
    draw = ImageDraw.Draw(img)

    # 배경 그라디언트
    for y in range(240):
        t = y / 240
        draw.line([(0, y), (239, y)],
                  fill=(int(10 + 15 * t), int(12 + 18 * t), int(30 + 35 * t)))

    f_title = _get_font(13)
    f_big   = _get_font(42)
    f_small = _get_font(12)
    f_label = _get_font(11)

    # 헤더
    draw.rectangle([0, 0, 239, 34], fill=(15, 25, 60))
    draw.text((10, 9),  "🖥  Pi5 Monitor",        fill=(180, 190, 255), font=f_title)
    draw.text((168, 9), now.strftime("%H:%M:%S"), fill=(80, 90, 140),   font=f_label)

    # CPU 수치 (대형)
    cpu_str = f"{cpu:.0f}%"
    bb = draw.textbbox((0, 0), cpu_str, font=f_big)
    cx = (240 - (bb[2] - bb[0])) // 2
    draw.text((cx + 2, 42), cpu_str, fill=(0, 0, 0),       font=f_big)
    draw.text((cx,     40), cpu_str, fill=_val_color(cpu), font=f_big)

    # CPU Load 레이블 + 바
    bb2 = draw.textbbox((0, 0), "CPU Load", font=f_label)
    cx2 = (240 - (bb2[2] - bb2[0])) // 2
    draw.text((cx2, 88), "CPU Load", fill=(100, 110, 160), font=f_label)
    _draw_bar(draw, 10, 104, 220, 12, cpu, _bar_color(cpu))
    draw.text((10, 122), f"🌡 {cpu_temp:.1f}°C", fill=(200, 180, 100), font=f_small)
    draw.line([(10, 143), (230, 143)], fill=(35, 45, 80), width=1)

    # 메모리
    draw.text((10, 148), "MEM",  fill=(120, 130, 180), font=f_label)
    draw.text((45, 148), f"{mem_used:.1f}/{mem_total:.1f}GB ({mem_pct:.0f}%)",
              fill=(180, 190, 230), font=f_label)
    _draw_bar(draw, 10, 161, 220, 10, mem_pct, _bar_color(mem_pct))

    # 디스크
    draw.text((10, 178), "DISK", fill=(120, 130, 180), font=f_label)
    draw.text((45, 178), f"{disk.used/(1024**3):.0f}/{disk.total/(1024**3):.0f}GB ({disk_pct:.0f}%)",
              fill=(180, 190, 230), font=f_label)
    _draw_bar(draw, 10, 191, 220, 10, disk_pct, _bar_color(disk_pct))

    # 푸터
    draw.rectangle([0, 210, 239, 239], fill=(8, 12, 28))
    uptime_sec = int(time.time() - psutil.boot_time())
    h, r = divmod(uptime_sec, 3600)
    m, _ = divmod(r, 60)
    draw.text((10,  220), f"Uptime {h}h {m}m",     fill=(60, 70, 110), font=f_label)
    draw.text((160, 220), now.strftime("%m/%d %a"), fill=(60, 70, 110), font=f_label)

    return img
