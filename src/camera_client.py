# src/camera_client.py
# Pi 125 카메라 서버 HTTP 클라이언트

import http.client
import json
import os
import urllib.parse
import logging

logger = logging.getLogger(__name__)


class CameraClient:
    def __init__(self, server_url: str, save_dir: str, timeout: int = 20,
                 api_token: str = ""):
        parsed         = urllib.parse.urlparse(server_url)
        self.host      = parsed.hostname or "192.168.x.x"
        self.port      = parsed.port or 5050
        self.save_dir  = save_dir
        self.timeout   = timeout
        self.api_token = api_token
        os.makedirs(save_dir, exist_ok=True)

    def _headers(self) -> dict:
        return {"X-API-Key": self.api_token} if self.api_token else {}

    # ── 내부 HTTP 헬퍼 ────────────────────────────────────────

    def _get(self, path: str) -> tuple[int, bytes]:
        conn = http.client.HTTPConnection(self.host, self.port, timeout=self.timeout)
        conn.request("GET", path, headers=self._headers())
        resp = conn.getresponse()
        data = resp.read()
        conn.close()
        return resp.status, data

    def _post(self, path: str) -> tuple[int, dict]:
        conn = http.client.HTTPConnection(self.host, self.port, timeout=self.timeout)
        headers = {"Content-Length": "0", **self._headers()}
        conn.request("POST", path, headers=headers)
        resp = conn.getresponse()
        data = json.loads(resp.read().decode(errors="ignore"))
        conn.close()
        return resp.status, data

    def _delete(self, path: str) -> int:
        conn = http.client.HTTPConnection(self.host, self.port, timeout=self.timeout)
        conn.request("DELETE", path, headers=self._headers())
        resp = conn.getresponse()
        resp.read()
        conn.close()
        return resp.status

    # ── 공개 메서드 ───────────────────────────────────────────

    def is_online(self) -> bool:
        """Pi 125 카메라 서버 연결 확인"""
        try:
            status, _ = self._get("/health")
            return status == 200
        except Exception:
            return False

    def capture(self) -> str | None:
        """촬영 트리거 → Pi 116 로컬 저장 → 파일명 반환 (실패 시 None)"""
        try:
            status, data = self._post("/capture")
            if status != 200:
                logger.error(f"촬영 실패: HTTP {status} — {data.get('message', '')}")
                return None
            filename = data.get("filename")
            if not filename:
                logger.error("촬영 응답에 filename 없음")
                return None

            # Pi 125 → Pi 116 이미지 다운로드
            img_status, img_data = self._get(f"/photo/{filename}")
            if img_status != 200:
                logger.error(f"이미지 다운로드 실패: HTTP {img_status}")
                return None

            local_path = os.path.join(self.save_dir, filename)
            with open(local_path, "wb") as f:
                f.write(img_data)
            logger.info(f"사진 저장: {local_path} ({len(img_data)} bytes)")
            return filename

        except Exception as e:
            logger.error(f"카메라 캡처 오류: {e}")
            return None

    def list_local(self) -> list[str]:
        """Pi 116 로컬 저장 사진 목록 (시간순)"""
        try:
            return sorted(
                f for f in os.listdir(self.save_dir)
                if f.lower().endswith((".jpg", ".jpeg", ".png"))
            )
        except Exception:
            return []

    def delete_local(self, filename: str) -> bool:
        """Pi 116 로컬 사진 삭제"""
        if ".." in filename or "/" in filename:
            return False
        path = os.path.join(self.save_dir, filename)
        if os.path.exists(path):
            os.remove(path)
            logger.info(f"사진 삭제: {filename}")
            return True
        return False

    def delete_all_local(self) -> int:
        """Pi 116 로컬 사진 전체 삭제 → 삭제 수 반환"""
        count = sum(1 for f in self.list_local() if self.delete_local(f))
        logger.info(f"전체 삭제: {count}장")
        return count
