# src/web_config.py
# Flask 기반 웹 설정 UI 서버

from flask import Flask, request, jsonify, render_template, redirect, url_for, send_file
import json
import os
import logging
import datetime

logger = logging.getLogger(__name__)

CONFIG_FILE = os.path.join(os.path.dirname(os.path.dirname(__file__)), "config.json")

app = Flask(__name__, template_folder="../templates")

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
        "device_ip":            "192.168.219.119",
        "city":                 "Pyeongtaek-si",
        "api_key":              "",
        "temp_unit":            "metric",
        "time_format":          "24h",
        "refresh_interval_sec": 60,
        "weather_interval_min": 10,
        "push_timeout_sec":     8,
        "push_retries":         3,
        "night_mode": {
            "enabled": False,
            "start":   "23:00",
            "end":     "07:00"
        }
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

        save_config(config)

        # push_client IP 갱신
        if _push_client:
            _push_client.device_ip = config["device_ip"]

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


@app.route("/preview")
def preview():
    """현재 생성된 이미지 미리보기"""
    cache_path = os.path.join(
        os.path.dirname(os.path.dirname(__file__)), "cache", "current.jpg"
    )
    if os.path.exists(cache_path):
        return send_file(cache_path, mimetype="image/jpeg")
    return "이미지 없음", 404
