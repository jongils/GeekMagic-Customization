# src/push_client.py
# GeekMagic SmallTV Ultra HTTP API 클라이언트
# ESP8266 Content-Length 중복 헤더 버그 → http.client 로 우회

import http.client
import uuid
import os
import logging
import time
import json

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
        1) Photo Album 테마 전환
        2) POST /doUpload → 파일 업로드
        3) GET  /set?img= → 화면 표시 지정
        """
        if not os.path.exists(image_path):
            logger.error(f"이미지 파일 없음: {image_path}")
            return False

        for attempt in range(1, self.retries + 1):
            try:
                # ── 1단계: Photo Album 테마 전환 ───────────────
                self.set_photo_album_theme()
                time.sleep(0.5)

                # ── 2단계: 이미지 업로드 ────────────────────────
                status, _ = self._post_file(
                    "/doUpload?dir=/image/",
                    image_path,
                    UPLOAD_FILENAME,
                )
                if status != 200:
                    logger.warning(f"업로드 실패 (시도 {attempt}/{self.retries}): HTTP {status}")
                    time.sleep(2)
                    continue
                logger.debug("업로드 성공")

                # ── 3단계: 화면 표시 지정 ───────────────────────
                status2, body2 = self._get(f"/set?img={UPLOAD_PATH}")
                if status2 == 200:
                    logger.info("장치 화면 업데이트 완료 ✅")
                    return True
                logger.warning(f"화면 지정 실패: HTTP {status2} / {body2.strip()}")

            except Exception as e:
                logger.error(f"Push 오류 (시도 {attempt}/{self.retries}): {e}")

            if attempt < self.retries:
                time.sleep(3)

        logger.error(f"Push 최종 실패 ({self.retries}회 시도)")
        return False

    def _post_json(self, path: str, data: dict) -> tuple[int, str]:
        """JSON POST 요청"""
        body = json.dumps(data).encode()
        conn = http.client.HTTPConnection(self.device_ip, timeout=self.timeout)
        conn.request(
            "POST", path, body=body,
            headers={
                "Content-Type":   "application/json",
                "Content-Length": str(len(body)),
            },
        )
        resp   = conn.getresponse()
        status = resp.status
        text   = resp.read().decode(errors="ignore")
        conn.close()
        return status, text

    def push_temp(self, temp_c: float | None = None) -> bool:
        """Pi CPU 온도를 ESP8266으로 전송 (POST /temp)"""
        if temp_c is None:
            temp_c = get_cpu_temp()
        if temp_c is None:
            logger.warning("CPU 온도 읽기 실패")
            return False
        try:
            status, body = self._post_json("/temp", {"c": round(temp_c, 1)})
            if status == 200:
                logger.debug(f"온도 전송: {temp_c:.1f}°C")
                return True
            logger.warning(f"온도 전송 실패: HTTP {status} / {body.strip()}")
        except Exception as e:
            logger.error(f"온도 전송 오류: {e}")
        return False

    def is_online(self) -> bool:
        """장치 온라인 여부 확인"""
        try:
            status, _ = self._get("/status")
            return status == 200
        except Exception:
            return False


def get_cpu_temp() -> float | None:
    """Raspberry Pi CPU 온도 읽기 (°C)"""
    try:
        with open("/sys/class/thermal/thermal_zone0/temp") as f:
            return int(f.read().strip()) / 1000.0
    except Exception:
        return None


if __name__ == "__main__":
    import argparse
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(message)s")

    parser = argparse.ArgumentParser(description="Pi CPU 온도 → ESP8266 전송")
    parser.add_argument("--ip",       default="192.168.219.122", help="장치 IP")
    parser.add_argument("--interval", type=int, default=30,      help="전송 주기 (초, 기본 30)")
    parser.add_argument("--once",     action="store_true",       help="한 번만 전송하고 종료")
    args = parser.parse_args()

    client = GeekMagicClient({"device_ip": args.ip})

    if args.once:
        t = get_cpu_temp()
        print(f"CPU 온도: {t:.1f}°C" if t is not None else "온도 읽기 실패")
        client.push_temp(t)
    else:
        print(f"온도 전송 시작 (IP={args.ip}, 주기={args.interval}s) — Ctrl+C로 종료")
        while True:
            client.push_temp()
            time.sleep(args.interval)
