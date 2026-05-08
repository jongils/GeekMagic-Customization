#!/usr/bin/env python3
# main.py — Weather Clock 메인 진입점

import os
import sys
import json
import logging
from datetime import datetime

# ── 로깅 설정 ─────────────────────────────────────────────────
LOG_DIR = os.path.join(os.path.dirname(__file__), "logs")
os.makedirs(LOG_DIR, exist_ok=True)

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
    handlers=[
        logging.StreamHandler(sys.stdout),
        logging.FileHandler(os.path.join(LOG_DIR, "weather-clock.log"), encoding="utf-8"),
    ],
)
logger = logging.getLogger("main")

# ── 경로 설정 ─────────────────────────────────────────────────
BASE_DIR    = os.path.dirname(os.path.abspath(__file__))
CONFIG_FILE = os.path.join(BASE_DIR, "config.json")
sys.path.insert(0, BASE_DIR)

# ── 기본 설정 ─────────────────────────────────────────────────
DEFAULT_CONFIG = {
    "device_ip":            "192.168.219.119",
    "city":                 "Pyeongtaek-si",
    "api_key":              "",
    "temp_unit":            "metric",
    "time_format":          "24h",
    "refresh_interval_sec": 60,
    "weather_interval_min": 10,
    "push_timeout_sec":     8,
    "push_retries":         3,
    "web_port":             8080,
    "night_mode": {
        "enabled": False,
        "start":   "23:00",
        "end":     "07:00"
    },
    "cpu_monitor": {
        "enabled":      False,
        "show_sec":     10,
        "interval_sec": 60,
    },
}


def load_config() -> dict:
    if os.path.exists(CONFIG_FILE):
        try:
            with open(CONFIG_FILE) as f:
                cfg = json.load(f)
            # 누락된 키는 기본값으로 채움
            for k, v in DEFAULT_CONFIG.items():
                cfg.setdefault(k, v)
            return cfg
        except Exception as e:
            logger.error(f"설정 로드 실패: {e} — 기본값 사용")
    else:
        # 최초 실행: 기본 설정 파일 생성
        with open(CONFIG_FILE, "w") as f:
            json.dump(DEFAULT_CONFIG, f, indent=2, ensure_ascii=False)
        logger.info(f"기본 설정 파일 생성: {CONFIG_FILE}")

    return DEFAULT_CONFIG.copy()


def main():
    logger.info("=" * 50)
    logger.info("  Weather Clock 시작")
    logger.info(f"  {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    logger.info("=" * 50)

    config = load_config()
    logger.info(f"설정 로드: 장치={config['device_ip']}, 도시={config['city']}")

    # ── 모듈 임포트 ───────────────────────────────────────────
    from src.weather_api     import WeatherAPI
    from src.image_generator import generate_weather_clock
    from src.push_client     import GeekMagicClient
    from src.scheduler       import WeatherClockScheduler
    from src.web_config      import app as flask_app, init_app, load_config as web_load_config

    # ── 객체 생성 ─────────────────────────────────────────────
    weather_api  = WeatherAPI(config)
    push_client  = GeekMagicClient(config)
    scheduler    = WeatherClockScheduler(
        config, weather_api, generate_weather_clock, push_client
    )

    init_app(scheduler, push_client)

    # ── 장치 연결 확인 ────────────────────────────────────────
    if push_client.is_online():
        logger.info(f"✅ 장치 연결 확인: {config['device_ip']}")
    else:
        logger.warning(f"⚠️  장치 오프라인: {config['device_ip']} — 연결 대기 중")

    # ── 스케줄러 백그라운드 시작 ──────────────────────────────
    scheduler.start_background()

    # ── Flask 웹 서버 시작 ────────────────────────────────────
    port = config.get("web_port", 8080)
    logger.info(f"웹 설정 UI: http://192.168.219.116:{port}")
    flask_app.run(
        host="0.0.0.0",
        port=port,
        debug=False,
        use_reloader=False,
    )


if __name__ == "__main__":
    main()
