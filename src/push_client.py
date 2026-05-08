# src/push_client.py
# GeekMagic SmallTV Ultra HTTP API 클라이언트
# ESP8266 Content-Length 중복 헤더 버그 → http.client 로 우회

import http.client
import uuid
import os
import logging
import time

logger = logging.getLogger(__name__)

UPLOAD_FILENAME = "weather_clock.jpg"
UPLOAD_PATH     = f"/image//{UPLOAD_FILENAME}"


class GeekMagicClient:
    def __init__(self, config: dict):
        self.device_ip = config.get("device_ip", "192.168.219.119")
        self.timeout   = config.get("push_timeout_sec", 10)
        self.retries   = config.get("push_retries", 3)

    def _get(self, path: str) -> tuple[int, str]:
        """GET 요청"""
        conn = http.client.HTTPConnection(self.device_ip, timeout=self.timeout)
        conn.request("GET", path)
        resp   = conn.getresponse()
        status = resp.status
        text   = resp.read().decode(errors="ignore")
        conn.close()
        return status, text

    def _post_file(self, path: str, filepath: str, filename: str) -> tuple[int, str]:
        """multipart/form-data 파일 업로드"""
        boundary = uuid.uuid4().hex
        with open(filepath, "rb") as f:
            file_data = f.read()

        body = (
            f"--{boundary}\r\n"
            f'Content-Disposition: form-data; name="file"; filename="{filename}"\r\n'
            f"Content-Type: image/jpeg\r\n\r\n"
        ).encode() + file_data + f"\r\n--{boundary}--\r\n".encode()

        conn = http.client.HTTPConnection(self.device_ip, timeout=self.timeout)
        conn.request(
            "POST", path, body=body,
            headers={
                "Content-Type":   f"multipart/form-data; boundary={boundary}",
                "Content-Length": str(len(body)),
            },
        )
        resp   = conn.getresponse()
        status = resp.status
        text   = resp.read().decode(errors="ignore")
        conn.close()
        return status, text

    def set_photo_album_theme(self) -> bool:
        """Photo Album 테마(theme=3)로 전환 — 업로드 이미지 표시에 필수"""
        try:
            status, body = self._get("/set?theme=3")
            if status == 200:
                logger.info("Photo Album 테마 전환 완료")
                return True
            logger.warning(f"테마 전환 실패: HTTP {status}")
        except Exception as e:
            logger.error(f"테마 전환 오류: {e}")
        return False

    def push_image(self, image_path: str) -> bool:
        """
        이미지를 장치에 업로드하고 화면에 표시
        1) POST /doUpload    → 파일 업로드 (테마와 무관하게 먼저 전송)
        2) GET  /set?theme=3 → Photo Album 모드 전환 (장치가 이미지 목록 초기화)
        3) GET  /set?img=    → 즉시 이미지 지정 (sleep 없이 연속 실행)

        Note: theme=3&img= 조합 파라미터는 ESP8266 펌웨어가 img를 무시함.
              theme 전환과 img 지정은 반드시 별도 요청으로 분리해야 함.
        """
        if not os.path.exists(image_path):
            logger.error(f"이미지 파일 없음: {image_path}")
            return False

        for attempt in range(1, self.retries + 1):
            try:
                # ── 1단계: 이미지 업로드 ────────────────────────────
                status, _ = self._post_file(
                    "/doUpload?dir=/image/",
                    image_path,
                    UPLOAD_FILENAME,
                )
                if status != 200:
                    logger.warning(f"업로드 실패 (시도 {attempt}/{self.retries}): HTTP {status}")
                    time.sleep(2)
                    continue

                # ── 2단계: Photo Album 테마 전환 ─────────────────────
                status2, _ = self._get("/set?theme=3")
                if status2 != 200:
                    logger.warning(f"테마 전환 실패: HTTP {status2}")
                    time.sleep(2)
                    continue

                # ── 3단계: 이미지 즉시 지정 (sleep 없이 연속 실행) ──
                status3, body3 = self._get(f"/set?img={UPLOAD_PATH}")
                if status3 == 200:
                    logger.info("장치 화면 업데이트 완료 ✅")
                    return True
                logger.warning(f"이미지 지정 실패: HTTP {status3} / {body3.strip()}")

            except Exception as e:
                logger.error(f"Push 오류 (시도 {attempt}/{self.retries}): {e}")

            if attempt < self.retries:
                time.sleep(3)

        logger.error(f"Push 최종 실패 ({self.retries}회 시도)")
        return False

    def set_theme(self, theme: int) -> bool:
        """장치 내장 테마 전환 (이미지 업로드 없이 테마만 변경)"""
        try:
            status, _ = self._get(f"/set?theme={theme}")
            if status == 200:
                logger.info(f"테마 {theme} 전환 완료")
                return True
            logger.warning(f"테마 전환 실패: HTTP {status}")
        except Exception as e:
            logger.error(f"테마 전환 오류: {e}")
        return False

    def is_online(self) -> bool:
        """장치 온라인 여부 확인"""
        try:
            status, _ = self._get("/")
            return status in (200, 302)
        except Exception:
            return False
