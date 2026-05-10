#!/usr/bin/env python3
# test_display.py — 장치 화면에 TEST 텍스트 표시 테스트

from PIL import Image, ImageDraw, ImageFont
import http.client
import uuid
import sys
import os

DEVICE_IP   = "192.168.x.x"   # config.json의 device_ip로 교체하세요
FILENAME    = "test_text.jpg"
OUTPUT_PATH = "/tmp/test_text.jpg"


def esp_get(host, path):
    conn   = http.client.HTTPConnection(host, timeout=10)
    conn.request("GET", path)
    resp   = conn.getresponse()
    status = resp.status
    text   = resp.read().decode(errors="ignore")
    conn.close()
    return status, text


def esp_post_file(host, path, filepath, filename):
    boundary = uuid.uuid4().hex
    with open(filepath, "rb") as f:
        file_data = f.read()
    body = (
        f"--{boundary}\r\n"
        f'Content-Disposition: form-data; name="file"; filename="{filename}"\r\n'
        f"Content-Type: image/jpeg\r\n\r\n"
    ).encode() + file_data + f"\r\n--{boundary}--\r\n".encode()
    conn = http.client.HTTPConnection(host, timeout=10)
    conn.request("POST", path, body=body,
        headers={"Content-Type": f"multipart/form-data; boundary={boundary}",
                 "Content-Length": str(len(body))})
    resp   = conn.getresponse()
    status = resp.status
    text   = resp.read().decode(errors="ignore")
    conn.close()
    return status, text


def make_image():
    img  = Image.new("RGB", (240, 240))
    draw = ImageDraw.Draw(img)
    for y in range(240):
        t = y / 240
        draw.line([(0, y), (239, y)],
                  fill=(int(12+20*t), int(18+30*t), int(42+50*t)))

    font_big = font_small = None
    for path in [
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSansBold.ttf",
    ]:
        if os.path.exists(path):
            font_big   = ImageFont.truetype(path, 72)
            font_small = ImageFont.truetype(path, 18)
            break

    text = "TEST"
    if font_big:
        bb = draw.textbbox((0, 0), text, font=font_big)
        x  = (240 - (bb[2] - bb[0])) // 2
        y  = (240 - (bb[3] - bb[1])) // 2 - 20
        draw.text((x+3, y+3), text, fill=(0, 0, 0),      font=font_big)
        draw.text((x,   y),   text, fill=(255, 220, 50), font=font_big)
    else:
        draw.text((70, 100), text, fill=(255, 220, 50))

    if font_small:
        sub = "GeekMagic Display Test"
        bb2 = draw.textbbox((0, 0), sub, font=font_small)
        x2  = (240 - (bb2[2] - bb2[0])) // 2
        draw.text((x2, 190), sub, fill=(100, 120, 200), font=font_small)

    img.save(OUTPUT_PATH, "JPEG", quality=90)
    print(f"✅ 이미지 생성: {OUTPUT_PATH}")


def push_to_device():
    import time

    # 1) Photo Album 테마로 전환 (필수!)
    print("🎨 Photo Album 테마 전환 중...")
    s, b = esp_get(DEVICE_IP, "/set?theme=3")
    print(f"   HTTP {s} / {b.strip()}")
    time.sleep(0.5)

    # 2) 업로드
    print(f"📤 업로드 중 → http://{DEVICE_IP}/doUpload?dir=/image/")
    status, body = esp_post_file(
        DEVICE_IP, "/doUpload?dir=/image/", OUTPUT_PATH, FILENAME
    )
    print(f"   HTTP {status}")
    if status != 200:
        print(f"❌ 업로드 실패\n   응답: {body[:300]}")
        sys.exit(1)
    print("✅ 업로드 성공")

    # 3) 화면 표시
    img_path = f"/image//{FILENAME}"
    print(f"🖥  화면 표시 → /set?img={img_path}")
    status2, body2 = esp_get(DEVICE_IP, f"/set?img={img_path}")
    print(f"   HTTP {status2} / 응답: {body2.strip()}")

    if status2 == 200:
        print("🎉 장치 화면에 TEST가 표시되었습니다!")
    else:
        print("❌ 화면 표시 실패")


if __name__ == "__main__":
    make_image()
    push_to_device()
