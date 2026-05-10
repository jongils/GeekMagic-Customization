# src/console_image.py
# 명령어 출력을 240×240 이미지로 렌더링

import os
import subprocess
import textwrap
import logging
from PIL import Image, ImageDraw, ImageFont

logger = logging.getLogger(__name__)

FONT_PATH  = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
FONT_BOLD  = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"
FONT_SIZE  = 10          # 본문 폰트 크기
TITLE_SIZE = 11          # 타이틀 폰트 크기
LINE_H     = 12          # 줄 높이 (px)
TITLE_H    = 20          # 타이틀 바 높이 (px)
PADDING    = 3           # 좌우 여백 (px)
IMG_W      = 240
IMG_H      = 240

# 사용 가능한 줄 수 계산
USABLE_H   = IMG_H - TITLE_H - PADDING
MAX_LINES  = USABLE_H // LINE_H          # ≈ 18줄

# 한 줄 최대 문자 수 (폰트 너비 기준 근사)
CHARS_PER_LINE = (IMG_W - PADDING * 2) // 6   # ≈ 39자


def _get_font(path: str, size: int) -> ImageFont.FreeTypeFont:
    try:
        return ImageFont.truetype(path, size)
    except Exception:
        return ImageFont.load_default()


def run_command(command: str, timeout: int = 8) -> str:
    """셸 명령어를 실행하고 출력(stdout+stderr)을 반환."""
    try:
        result = subprocess.run(
            command,
            shell=True,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        output = result.stdout
        if result.returncode != 0 and result.stderr:
            output += result.stderr
        return output
    except subprocess.TimeoutExpired:
        return f"[타임아웃: {timeout}초 초과]"
    except Exception as e:
        return f"[실행 오류: {e}]"


def _wrap_lines(raw_output: str) -> list:
    """출력 문자열을 화면 너비에 맞게 줄바꿈, 마지막 MAX_LINES 줄만 반환."""
    lines = []
    for raw_line in raw_output.splitlines():
        # 탭 → 공백 변환 후 줄바꿈
        raw_line = raw_line.replace("\t", "    ")
        if len(raw_line) <= CHARS_PER_LINE:
            lines.append(raw_line)
        else:
            wrapped = textwrap.wrap(raw_line, width=CHARS_PER_LINE,
                                    break_long_words=True, break_on_hyphens=False)
            lines.extend(wrapped if wrapped else [""])
    # 마지막 N줄만 표시 (스크롤 효과)
    return lines[-MAX_LINES:]


def generate_console_image(command: str, label: str = "") -> Image.Image:
    """명령어를 실행하고 출력을 240×240 이미지로 렌더링."""
    output  = run_command(command)
    lines   = _wrap_lines(output)

    img  = Image.new("RGB", (IMG_W, IMG_H), (8, 10, 22))
    draw = ImageDraw.Draw(img)

    # 배경 그라디언트 (subtle)
    for y in range(IMG_H):
        t = y / IMG_H
        r = int(8  + 10 * t)
        g = int(10 + 12 * t)
        b = int(22 + 20 * t)
        draw.line([(0, y), (IMG_W - 1, y)], fill=(r, g, b))

    # 타이틀 바
    draw.rectangle([0, 0, IMG_W - 1, TITLE_H - 1], fill=(15, 25, 60))
    title_font = _get_font(FONT_BOLD, TITLE_SIZE)
    title_text = label if label else command[:28]
    draw.text((PADDING, 4), f"❯ {title_text}", fill=(140, 180, 255), font=title_font)

    # 본문 라인
    body_font = _get_font(FONT_PATH, FONT_SIZE)
    y = TITLE_H + PADDING
    for line in lines:
        if y + LINE_H > IMG_H:
            break
        # 기본 흰색, 특정 키워드 강조
        color = _line_color(line)
        draw.text((PADDING, y), line, fill=color, font=body_font)
        y += LINE_H

    # 출력이 없을 때
    if not lines:
        draw.text((PADDING, TITLE_H + 10), "(출력 없음)", fill=(60, 80, 120), font=body_font)

    return img


def _line_color(line: str) -> tuple:
    """줄 내용에 따라 강조색 반환."""
    lo = line.lower()
    if any(k in lo for k in ("error", "fail", "fatal", "critical", "오류", "실패")):
        return (255, 100, 100)   # 빨강
    if any(k in lo for k in ("warn", "warning", "경고")):
        return (255, 200,  80)   # 노랑
    if any(k in lo for k in ("ok", "success", "✅", "완료", "started", "active")):
        return ( 80, 220, 120)   # 초록
    if line.startswith("  ") or line.startswith("\t"):
        return (160, 170, 200)   # 들여쓰기: 밝은 파랑
    return (200, 210, 230)       # 기본: 연한 흰색
