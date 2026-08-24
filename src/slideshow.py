# src/slideshow.py
# 로컬 폴더 슬라이드쇼 이미지 처리 모듈

import os
import random
import logging
from PIL import Image, ImageOps

logger = logging.getLogger(__name__)

SUPPORTED_EXTS = {".jpg", ".jpeg", ".png", ".bmp", ".webp"}
DISPLAY_SIZE   = (240, 240)


def load_image_list(folder: str, shuffle: bool = False) -> list:
    """폴더에서 지원 포맷 이미지 목록 반환.

    Args:
        folder:  스캔할 폴더 경로
        shuffle: True이면 랜덤 순서, False이면 파일명 오름차순
    Returns:
        절대 경로 리스트 (이미지 없으면 빈 리스트)
    """
    if not os.path.isdir(folder):
        logger.warning(f"슬라이드쇼 폴더 없음: {folder}")
        return []

    files = [
        os.path.join(folder, f)
        for f in os.listdir(folder)
        if os.path.splitext(f)[1].lower() in SUPPORTED_EXTS
        and os.path.isfile(os.path.join(folder, f))
    ]

    if not files:
        logger.warning(f"슬라이드쇼: 이미지 없음 ({folder})")
        return []

    files.sort()
    if shuffle:
        random.shuffle(files)

    logger.info(f"슬라이드쇼: {len(files)}장 로드 ({folder})")
    return files


def prepare_image(src_path: str, dst_path: str) -> bool:
    """원본 이미지를 240×240 JPEG로 변환 후 저장.

    처리 순서:
      1. EXIF 방향 자동 적용 (세로 사진 회전)
      2. RGB 변환 (RGBA·팔레트 모드 호환)
      3. 비율 유지 + 중앙 크롭 → 240×240
      4. JPEG quality=85로 저장

    Returns:
        성공 시 True, 실패 시 False
    """
    try:
        with Image.open(src_path) as img:
            img = ImageOps.exif_transpose(img)          # EXIF 회전 보정
            if img.mode != "RGB":
                img = img.convert("RGB")
            img = ImageOps.fit(img, DISPLAY_SIZE, Image.LANCZOS)  # 중앙 크롭
            img.save(dst_path, "JPEG", quality=85)
        return True
    except Exception as e:
        logger.error(f"이미지 처리 실패 ({os.path.basename(src_path)}): {e}")
        return False


def count_images(folder: str) -> int:
    """폴더의 이미지 파일 수 반환 (웹 UI 표시용)."""
    return len(load_image_list(folder))
