#!/usr/bin/env python3
# cpu_monitor.py — CPU 10초 표시 → 원래 테마 50초 → 반복
# 실행: venv/bin/python3 cpu_monitor.py  /  종료: Ctrl+C

from PIL import Image, ImageDraw, ImageFont
import http.client, uuid, time, os, psutil, datetime, pytz

DEVICE_IP   = "192.168.219.119"
FILENAME    = "weather_clock.jpg"   # 장치가 실제 사용하는 파일명
OUTPUT_PATH = "/tmp/weather_clock.jpg"
SHOW_SEC    = 10
RESTORE_SEC = 50
ORIG_THEME  = 1
KST         = pytz.timezone("Asia/Seoul")

# ── HTTP 헬퍼 ─────────────────────────────────────────────────
def esp_get(path):
    try:
        conn = http.client.HTTPConnection(DEVICE_IP, timeout=10)
        conn.request("GET", path)
        r = conn.getresponse()
        status = r.status
        r.read()
        conn.close()
        return status
    except Exception as e:
        print(f"  ⚠️  {e}")
        return 0

def esp_upload(filepath):
    try:
        boundary = uuid.uuid4().hex
        with open(filepath, "rb") as f:
            data = f.read()
        body = (
            f"--{boundary}\r\n"
            f'Content-Disposition: form-data; name="file"; filename="{FILENAME}"\r\n'
            f"Content-Type: image/jpeg\r\n\r\n"
        ).encode() + data + f"\r\n--{boundary}--\r\n".encode()
        conn = http.client.HTTPConnection(DEVICE_IP, timeout=10)
        conn.request("POST", "/doUpload?dir=/image/", body=body,
            headers={"Content-Type":   f"multipart/form-data; boundary={boundary}",
                     "Content-Length": str(len(body))})
        r = conn.getresponse()
        status = r.status
        r.read()
        conn.close()
        return status
    except Exception as e:
        print(f"  ⚠️  업로드 오류: {e}")
        return 0

# ── 폰트 ──────────────────────────────────────────────────────
def get_font(size):
    for p in [
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",
    ]:
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()

def val_color(v):
    if v < 50: return (80, 220, 100)
    if v < 80: return (255, 200, 50)
    return            (255, 80,  80)

def bar_color(v):
    if v < 50: return (40, 160, 70)
    if v < 80: return (180, 140, 30)
    return            (180, 50,  50)

def draw_bar(draw, x, y, w, h, val, color):
    draw.rectangle([x, y, x+w, y+h], fill=(30, 35, 60))
    fw = int(w * min(val, 100) / 100)
    if fw > 0:
        draw.rectangle([x, y, x+fw, y+h], fill=color)
    draw.rectangle([x, y, x+w, y+h], outline=(50, 60, 100), width=1)

# ── CPU 이미지 생성 ───────────────────────────────────────────
def make_cpu_image():
    cpu       = psutil.cpu_percent(interval=1)
    mem       = psutil.virtual_memory()
    mem_pct   = mem.percent
    mem_used  = mem.used  / (1024**3)
    mem_total = mem.total / (1024**3)
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

    for y in range(240):
        t = y / 240
        draw.line([(0, y), (239, y)],
                  fill=(int(10+15*t), int(12+18*t), int(30+35*t)))

    f_title = get_font(13)
    f_big   = get_font(42)
    f_small = get_font(12)
    f_label = get_font(11)

    draw.rectangle([0, 0, 239, 34], fill=(15, 25, 60))
    draw.text((10, 9),  "🖥  Pi5 Monitor",        fill=(180, 190, 255), font=f_title)
    draw.text((168, 9), now.strftime("%H:%M:%S"), fill=(80, 90, 140),   font=f_label)

    cpu_str = f"{cpu:.0f}%"
    bb = draw.textbbox((0, 0), cpu_str, font=f_big)
    cx = (240 - (bb[2] - bb[0])) // 2
    draw.text((cx+2, 42), cpu_str, fill=(0, 0, 0),      font=f_big)
    draw.text((cx,   40), cpu_str, fill=val_color(cpu), font=f_big)

    bb2 = draw.textbbox((0, 0), "CPU Load", font=f_label)
    cx2 = (240 - (bb2[2] - bb2[0])) // 2
    draw.text((cx2, 88), "CPU Load", fill=(100, 110, 160), font=f_label)
    draw_bar(draw, 10, 104, 220, 12, cpu, bar_color(cpu))
    draw.text((10, 122), f"🌡 {cpu_temp:.1f}°C", fill=(200, 180, 100), font=f_small)
    draw.line([(10, 143), (230, 143)], fill=(35, 45, 80), width=1)

    draw.text((10, 148), "MEM",  fill=(120, 130, 180), font=f_label)
    draw.text((45, 148), f"{mem_used:.1f}/{mem_total:.1f}GB ({mem_pct:.0f}%)",
              fill=(180, 190, 230), font=f_label)
    draw_bar(draw, 10, 161, 220, 10, mem_pct, bar_color(mem_pct))

    draw.text((10, 178), "DISK", fill=(120, 130, 180), font=f_label)
    draw.text((45, 178), f"{disk.used/(1024**3):.0f}/{disk.total/(1024**3):.0f}GB ({disk_pct:.0f}%)",
              fill=(180, 190, 230), font=f_label)
    draw_bar(draw, 10, 191, 220, 10, disk_pct, bar_color(disk_pct))

    draw.rectangle([0, 210, 239, 239], fill=(8, 12, 28))
    uptime_sec = int(time.time() - psutil.boot_time())
    h, r = divmod(uptime_sec, 3600)
    m, _ = divmod(r, 60)
    draw.text((10,  220), f"Uptime {h}h {m}m",     fill=(60, 70, 110), font=f_label)
    draw.text((160, 220), now.strftime("%m/%d %a"), fill=(60, 70, 110), font=f_label)

    img.save(OUTPUT_PATH, "JPEG", quality=90)
    return cpu, mem_pct, cpu_temp

# ── CPU 화면 표시 ─────────────────────────────────────────────
def show_cpu():
    cpu, mem, temp = make_cpu_image()
    ts = datetime.datetime.now(KST).strftime("%H:%M:%S")
    print(f"[{ts}] CPU {cpu:.0f}%  MEM {mem:.0f}%  TEMP {temp:.1f}°C")

    # 업로드 후 테마 전환+이미지 지정을 단일 요청으로 처리 → 중간 플래시 없음
    esp_upload(OUTPUT_PATH)
    esp_get(f"/set?theme=3&img=/image//{FILENAME}")
    print(f"  ✅ CPU 화면 ({SHOW_SEC}초)")

# ── 원래 테마 복원 ────────────────────────────────────────────
def restore_theme():
    ts = datetime.datetime.now(KST).strftime("%H:%M:%S")
    # ORIG_THEME(1)은 장치 내장 테마이므로 이미지 업로드 없이 테마 전환만으로 충분
    esp_get(f"/set?theme={ORIG_THEME}")
    print(f"[{ts}] ↩️  원래 테마 복원 ({RESTORE_SEC}초)")

# ── 메인 루프 ─────────────────────────────────────────────────
if __name__ == "__main__":
    print("=" * 45)
    print(f"  CPU Monitor  |  CPU {SHOW_SEC}초 → 원래 테마 {RESTORE_SEC}초")
    print(f"  장치: {DEVICE_IP}  |  종료: Ctrl+C")
    print("=" * 45)
    print()

    try:
        while True:
            show_cpu()
            time.sleep(SHOW_SEC)
            restore_theme()
            time.sleep(RESTORE_SEC)
    except KeyboardInterrupt:
        print("\n종료 — 원래 테마 복원 중...")
        restore_theme()
        print("완료")
