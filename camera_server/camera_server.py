#!/usr/bin/env python3
# camera_server.py — USB 카메라 캡처 서버
# Pi 125에서 실행: venv/bin/python3 camera_server.py
# 환경변수: CAMERA_DEV (기본 /dev/video0), PORT (기본 5050)

from flask import Flask, jsonify, send_file, abort, request
import subprocess
import os
import datetime
import logging
import secrets

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s"
)
logger = logging.getLogger(__name__)

app = Flask(__name__)

CAPTURE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "captures")
CAMERA_DEV  = os.environ.get("CAMERA_DEV", "/dev/video0")
CAPTURE_RES = os.environ.get("CAPTURE_RES", "1280x720")
PORT        = int(os.environ.get("PORT", 5050))
API_TOKEN   = os.environ.get("CAMERA_API_TOKEN", "")

os.makedirs(CAPTURE_DIR, exist_ok=True)


@app.before_request
def require_api_token():
    """모든 카메라 API 요청에 공유 토큰 인증을 적용한다."""
    if not API_TOKEN:
        return jsonify({"status": "error", "message": "CAMERA_API_TOKEN 미설정"}), 503
    supplied = request.headers.get("X-API-Key", "")
    if not secrets.compare_digest(supplied, API_TOKEN):
        return jsonify({"status": "error", "message": "unauthorized"}), 401


def _safe_filename(filename: str) -> bool:
    """경로 조작 방지"""
    return filename and ".." not in filename and "/" not in filename


def capture_photo() -> tuple[bool, str]:
    """USB 카메라로 사진 촬영 → (성공 여부, 파일명) 반환"""
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    filename  = f"cam_{timestamp}.jpg"
    filepath  = os.path.join(CAPTURE_DIR, filename)

    try:
        result = subprocess.run(
            ["fswebcam", "-d", CAMERA_DEV, "-r", CAPTURE_RES,
             "--no-banner", "--quiet", "--skip", "3", filepath],
            capture_output=True, text=True, timeout=20
        )
        if result.returncode == 0 and os.path.exists(filepath):
            size = os.path.getsize(filepath)
            logger.info(f"촬영 완료: {filename} ({size} bytes)")
            return True, filename
        logger.error(f"촬영 실패 (returncode={result.returncode}): {result.stderr.strip()}")
        return False, ""
    except FileNotFoundError:
        logger.error("fswebcam 미설치 — sudo apt install fswebcam")
        return False, "fswebcam not found"
    except subprocess.TimeoutExpired:
        logger.error("촬영 타임아웃 (20초)")
        return False, "timeout"
    except Exception as e:
        logger.error(f"촬영 오류: {e}")
        return False, str(e)


# ── 엔드포인트 ─────────────────────────────────────────────────

@app.route("/health")
def health():
    """연결 상태 확인"""
    return jsonify({
        "status":     "ok",
        "camera_dev": CAMERA_DEV,
        "capture_dir": CAPTURE_DIR,
    })


@app.route("/capture", methods=["POST"])
def capture():
    """사진 촬영 트리거"""
    success, result = capture_photo()
    if success:
        return jsonify({"status": "ok", "filename": result})
    return jsonify({"status": "error", "message": result}), 500


@app.route("/photos")
def list_photos():
    """촬영된 사진 목록 반환 (시간순 정렬)"""
    files = sorted(
        f for f in os.listdir(CAPTURE_DIR)
        if f.lower().endswith((".jpg", ".jpeg", ".png"))
    )
    return jsonify({"photos": files, "count": len(files)})


@app.route("/photo/<filename>")
def serve_photo(filename):
    """사진 파일 반환"""
    if not _safe_filename(filename):
        abort(400)
    path = os.path.join(CAPTURE_DIR, filename)
    if not os.path.exists(path):
        abort(404)
    return send_file(path, mimetype="image/jpeg")


@app.route("/delete/<filename>", methods=["DELETE"])
def delete_photo(filename):
    """사진 삭제"""
    if not _safe_filename(filename):
        abort(400)
    path = os.path.join(CAPTURE_DIR, filename)
    if not os.path.exists(path):
        abort(404)
    os.remove(path)
    logger.info(f"삭제: {filename}")
    return jsonify({"status": "ok"})


@app.route("/delete-all", methods=["DELETE"])
def delete_all():
    """전체 사진 삭제"""
    count = 0
    for f in os.listdir(CAPTURE_DIR):
        if f.lower().endswith((".jpg", ".jpeg", ".png")):
            os.remove(os.path.join(CAPTURE_DIR, f))
            count += 1
    logger.info(f"전체 삭제: {count}장")
    return jsonify({"status": "ok", "deleted": count})


if __name__ == "__main__":
    logger.info(f"카메라 서버 시작 — 장치: {CAMERA_DEV}, 포트: {PORT}")
    logger.info(f"저장 폴더: {CAPTURE_DIR}")
    app.run(host="0.0.0.0", port=PORT, debug=False)
