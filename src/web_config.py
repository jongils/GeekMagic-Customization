# src/web_config.py
# Flask 기반 웹 설정 UI 서버

from flask import Flask, request, jsonify, render_template, redirect, url_for, send_file
import json
import os
import logging
import datetime
import secrets

logger = logging.getLogger(__name__)

CONFIG_FILE = os.path.join(os.path.dirname(os.path.dirname(__file__)), "config.json")

app = Flask(__name__, template_folder="../templates")


@app.before_request
def require_web_auth():
    """모든 웹 UI/API 요청에 HTTP Basic 인증을 적용한다."""
    config = load_config()
    auth_cfg = config.get("web_auth", {})
    username = str(auth_cfg.get("username", "admin"))
    password = str(auth_cfg.get("password", ""))

    if not password:
        return ("web_auth.password가 설정되지 않았습니다", 503)

    auth = request.authorization
    valid = bool(
        auth
        and secrets.compare_digest(auth.username or "", username)
        and secrets.compare_digest(auth.password or "", password)
    )
    if not valid:
        return ("인증이 필요합니다", 401,
                {"WWW-Authenticate": 'Basic realm="Weather Clock"'})


# 전역 참조 (main.py에서 주입)
_scheduler   = None
_push_client = None


def init_app(scheduler, push_client):
    global _scheduler, _push_client
    _scheduler   = scheduler
    _push_client = push_client


def load_config() -> dict:
    try:
        with open(CONFIG_FILE) as f:
            return json.load(f)
    except Exception:
        return get_default_config()


def save_config(data: dict):
    with open(CONFIG_FILE, "w") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)


def get_default_config() -> dict:
    return {
        "device_ip":            "192.168.x.x",
        "city":                 "Seoul",
        "api_key":              "",
        "temp_unit":            "metric",
        "time_format":          "24h",
        "refresh_interval_sec": 60,
        "weather_interval_min": 10,
        "push_timeout_sec":     8,
        "push_retries":         3,
        "web_auth": {
            "username": "admin",
            "password": "",
        },
        "night_mode": {
            "enabled": False,
            "start":   "23:00",
            "end":     "07:00"
        },
        "cpu_monitor": {
            "enabled":       False,
            "show_sec":      10,
            "rest_sec":      50,
            "restore_theme": 0,
        },
        "slideshow": {
            "enabled":       False,
            "folder":        "/home/pi/Pictures",
            "show_sec":      10,
            "rest_sec":      0,
            "shuffle":       False,
            "restore_theme": 1,
        },
        "console": {
            "enabled":       False,
            "command":       "vcgencmd measure_temp && free -h && df -h /",
            "label":         "",
            "refresh_sec":   5,
            "show_sec":      30,
            "rest_sec":      0,
            "restore_theme": 1,
        },
        "camera": {
            "enabled":       False,
            "server_url":    "http://192.168.x.x:5050",
            "api_token":     "",
            "show_sec":      10,
            "restore_theme": 1,
        },
    }


# ── 라우트 ────────────────────────────────────────────────────

@app.route("/")
def index():
    config = load_config()
    device_online = _push_client.is_online() if _push_client else False
    return render_template("index.html",
                           config=config,
                           device_online=device_online)


@app.route("/config", methods=["POST"])
def save_config_route():
    try:
        data = request.form.to_dict()
        config = load_config()

        config["device_ip"]            = data.get("device_ip",  config["device_ip"])
        config["city"]                 = data.get("city",        config["city"])
        config["api_key"]              = data.get("api_key",     config["api_key"])
        config["temp_unit"]            = data.get("temp_unit",   "metric")
        config["time_format"]          = data.get("time_format", "24h")
        config["refresh_interval_sec"] = int(data.get("refresh_interval_sec", 60))
        config["weather_interval_min"] = int(data.get("weather_interval_min", 10))
        config["night_mode"]["enabled"] = data.get("night_mode_enabled") == "on"
        config["night_mode"]["start"]   = data.get("night_mode_start", "23:00")
        config["night_mode"]["end"]     = data.get("night_mode_end",   "07:00")

        if "cpu_monitor" not in config:
            config["cpu_monitor"] = {}
        config["cpu_monitor"]["enabled"]       = data.get("cpu_monitor_enabled") == "on"
        config["cpu_monitor"]["show_sec"]      = int(data.get("cpu_monitor_show_sec",  10))
        config["cpu_monitor"]["rest_sec"]      = int(data.get("cpu_monitor_rest_sec",  50))
        config["cpu_monitor"]["restore_theme"] = int(data.get("cpu_monitor_restore_theme", 0))

        if "slideshow" not in config:
            config["slideshow"] = {}
        config["slideshow"]["enabled"]       = data.get("slideshow_enabled") == "on"
        config["slideshow"]["folder"]        = data.get("slideshow_folder", "/home/pi/Pictures").strip()
        config["slideshow"]["show_sec"]      = int(data.get("slideshow_show_sec",  10))
        config["slideshow"]["rest_sec"]      = int(data.get("slideshow_rest_sec",   0))
        config["slideshow"]["shuffle"]       = data.get("slideshow_shuffle") == "on"
        config["slideshow"]["restore_theme"] = int(data.get("slideshow_restore_theme", 1))

        if "console" not in config:
            config["console"] = {}
        config["console"]["enabled"]       = data.get("console_enabled") == "on"
        config["console"]["command"]       = data.get("console_command", "").strip()
        config["console"]["label"]         = data.get("console_label", "").strip()
        config["console"]["refresh_sec"]   = int(data.get("console_refresh_sec",  5))
        config["console"]["show_sec"]      = int(data.get("console_show_sec",    30))
        config["console"]["rest_sec"]      = int(data.get("console_rest_sec",     0))
        config["console"]["restore_theme"] = int(data.get("console_restore_theme", 1))

        if "camera" not in config:
            config["camera"] = {}
        config["camera"]["enabled"]       = data.get("camera_enabled") == "on"
        config["camera"]["server_url"]    = data.get("camera_server_url", "http://192.168.x.x:5050").strip()
        config["camera"]["show_sec"]      = int(data.get("camera_show_sec", 10))
        config["camera"]["restore_theme"] = int(data.get("camera_restore_theme", 1))

        save_config(config)

        # push_client IP 갱신
        if _push_client:
            _push_client.device_ip = config["device_ip"]

        # 스케줄러 config 실시간 갱신 (재시작 없이 반영)
        if _scheduler:
            _scheduler.config.update(config)

            import threading

            # CPU 모니터 스레드 동적 시작 (설정 저장 시 활성화된 경우)
            if (config.get("cpu_monitor", {}).get("enabled", False)
                    and (_scheduler._cpu_thread is None
                         or not _scheduler._cpu_thread.is_alive())):
                _scheduler._cpu_thread = threading.Thread(
                    target=_scheduler._cpu_monitor_loop,
                    daemon=True,
                    name="cpu-monitor",
                )
                _scheduler._cpu_thread.start()
                logger.info("CPU 모니터 스레드 동적 시작")

            # 슬라이드쇼 스레드 동적 시작 (설정 저장 시 활성화된 경우)
            if (config.get("slideshow", {}).get("enabled", False)
                    and (_scheduler._slideshow_thread is None
                         or not _scheduler._slideshow_thread.is_alive())):
                _scheduler._slideshow_thread = threading.Thread(
                    target=_scheduler._slideshow_loop,
                    daemon=True,
                    name="slideshow",
                )
                _scheduler._slideshow_thread.start()
                logger.info("슬라이드쇼 스레드 동적 시작")

            # 콘솔 스레드 동적 시작 (설정 저장 시 활성화된 경우)
            if (config.get("console", {}).get("enabled", False)
                    and (_scheduler._console_thread is None
                         or not _scheduler._console_thread.is_alive())):
                _scheduler._console_thread = threading.Thread(
                    target=_scheduler._console_loop,
                    daemon=True,
                    name="console",
                )
                _scheduler._console_thread.start()
                logger.info("콘솔 스레드 동적 시작")

            # 카메라 슬라이드쇼 스레드 동적 시작 (설정 저장 시 활성화된 경우)
            if (config.get("camera", {}).get("enabled", False)
                    and (_scheduler._camera_thread is None
                         or not _scheduler._camera_thread.is_alive())):
                _scheduler._camera_thread = threading.Thread(
                    target=_scheduler._camera_loop,
                    daemon=True,
                    name="camera",
                )
                _scheduler._camera_thread.start()
                logger.info("카메라 슬라이드쇼 스레드 동적 시작")

        logger.info("설정 저장 완료")
        return redirect(url_for("index") + "?saved=1")

    except Exception as e:
        logger.error(f"설정 저장 실패: {e}")
        return jsonify({"error": str(e)}), 500


@app.route("/status")
def status():
    config = load_config()
    online = _push_client.is_online() if _push_client else False
    return jsonify({
        "device_ip":    config.get("device_ip"),
        "device_online": online,
        "city":         config.get("city"),
        "time":         datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
    })


@app.route("/push-now")
def push_now():
    """즉시 화면 업데이트 트리거"""
    if _scheduler:
        try:
            _scheduler._update_weather()
            _scheduler._update_display()
            return jsonify({"result": "ok", "message": "화면 업데이트 완료"})
        except Exception as e:
            return jsonify({"result": "error", "message": str(e)}), 500
    return jsonify({"result": "error", "message": "스케줄러 미초기화"}), 500


@app.route("/slideshow-info")
def slideshow_info():
    """슬라이드쇼 폴더 이미지 수 반환 (웹 UI 실시간 표시용).
    ?folder= 파라미터로 폴더 경로를 동적으로 지정 가능.
    """
    config = load_config()
    folder = request.args.get(
        "folder",
        config.get("slideshow", {}).get("folder", "/home/pi/Pictures")
    )
    try:
        from src.slideshow import count_images
        count = count_images(folder)
    except Exception:
        count = 0
    return jsonify({"count": count, "folder": folder})


@app.route("/preview")
def preview():
    """현재 생성된 이미지 미리보기"""
    cache_path = os.path.join(
        os.path.dirname(os.path.dirname(__file__)), "cache", "current.jpg"
    )
    if os.path.exists(cache_path):
        return send_file(cache_path, mimetype="image/jpeg")
    return "이미지 없음", 404


def _get_camera_dir() -> str:
    return os.path.join(os.path.dirname(os.path.dirname(__file__)), "cache", "camera_feed")


def _get_camera_client():
    config     = load_config()
    camera_cfg = config.get("camera", {})
    server_url = camera_cfg.get("server_url", "")
    api_token  = camera_cfg.get("api_token", "")
    save_dir   = _get_camera_dir()
    from src.camera_client import CameraClient
    return CameraClient(server_url, save_dir, api_token=api_token)


@app.route("/camera/status")
def camera_status():
    """Pi 125 카메라 서버 연결 상태 + 로컬 사진 수"""
    client = _get_camera_client()
    online = client.is_online()
    photos = client.list_local()
    return jsonify({"online": online, "count": len(photos)})


@app.route("/camera/capture", methods=["POST"])
def camera_capture():
    """촬영 트리거 — Pi 125 촬영 → Pi 116 다운로드"""
    client   = _get_camera_client()
    filename = client.capture()
    if filename:
        photos = client.list_local()
        return jsonify({"status": "ok", "filename": filename, "count": len(photos)})
    return jsonify({"status": "error", "message": "촬영 실패 — 카메라 서버 연결 확인"}), 500


@app.route("/camera/photos")
def camera_photos():
    """Pi 116 로컬 사진 목록"""
    client = _get_camera_client()
    photos = client.list_local()
    return jsonify({"photos": photos, "count": len(photos)})


@app.route("/camera/photo/<filename>")
def camera_photo(filename):
    """썸네일 이미지 반환"""
    if ".." in filename or "/" in filename:
        return "invalid", 400
    path = os.path.join(_get_camera_dir(), filename)
    if os.path.exists(path):
        return send_file(path, mimetype="image/jpeg")
    return "not found", 404


@app.route("/camera/delete/<filename>", methods=["DELETE"])
def camera_delete(filename):
    """사진 1장 삭제"""
    client = _get_camera_client()
    if client.delete_local(filename):
        photos = client.list_local()
        return jsonify({"status": "ok", "count": len(photos)})
    return jsonify({"status": "error"}), 404


@app.route("/camera/delete-all", methods=["DELETE"])
def camera_delete_all():
    """전체 사진 삭제"""
    client = _get_camera_client()
    count  = client.delete_all_local()
    return jsonify({"status": "ok", "deleted": count})


@app.route("/console-preview")
def console_preview():
    """콘솔 출력 이미지 즉시 생성 후 반환 (웹 UI 미리보기용).
    ?command= 파라미터로 명령어를 동적으로 지정 가능.
    """
    config  = load_config()
    command = request.args.get(
        "command",
        config.get("console", {}).get("command", "")
    ).strip()
    label = request.args.get(
        "label",
        config.get("console", {}).get("label", "")
    ).strip()

    if not command:
        return "명령어 없음", 400

    try:
        from src.console_image import generate_console_image
        img = generate_console_image(command, label)
        preview_path = os.path.join(
            os.path.dirname(os.path.dirname(__file__)), "cache", "console_preview.jpg"
        )
        os.makedirs(os.path.dirname(preview_path), exist_ok=True)
        img.save(preview_path, "JPEG", quality=90)
        return send_file(preview_path, mimetype="image/jpeg")
    except Exception as e:
        logger.error(f"콘솔 미리보기 생성 실패: {e}")
        return str(e), 500
