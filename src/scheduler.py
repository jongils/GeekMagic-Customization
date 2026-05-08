# src/scheduler.py
# 주기적 이미지 생성 및 Push 스케줄러

import schedule
import time
import os
import logging
import threading
from datetime import datetime

import pytz

logger = logging.getLogger(__name__)
KST = pytz.timezone("Asia/Seoul")


class WeatherClockScheduler:
    def __init__(self, config: dict, weather_api, image_gen_fn, push_client):
        self.config       = config
        self.weather_api  = weather_api
        self.gen_fn       = image_gen_fn   # generate_weather_clock 함수
        self.push_client  = push_client
        self.image_path   = os.path.join(
            os.path.dirname(os.path.dirname(__file__)), "cache", "current.jpg"
        )
        self._weather_cache = None
        self._running = False
        self._thread  = None

    def _is_night_mode(self) -> bool:
        """야간 모드 여부 확인"""
        nm = self.config.get("night_mode", {})
        if not nm.get("enabled", False):
            return False
        try:
            now = datetime.now(KST).strftime("%H:%M")
            start = nm.get("start", "23:00")
            end   = nm.get("end",   "07:00")
            # 자정 경계 처리
            if start > end:
                return now >= start or now < end
            return start <= now < end
        except Exception:
            return False

    def _update_weather(self):
        """날씨 데이터 갱신"""
        logger.info("날씨 데이터 갱신 중...")
        self._weather_cache = self.weather_api.get_weather()

    def _update_display(self):
        """이미지 생성 → 장치 Push"""
        if self._is_night_mode():
            logger.debug("야간 모드 — Push 건너뜀")
            return

        if not self.push_client.is_online():
            logger.warning("장치 오프라인 — Push 건너뜀")
            return

        # 날씨 데이터 없으면 즉시 조회
        if self._weather_cache is None:
            self._update_weather()

        try:
            img = self.gen_fn(self._weather_cache, self.config)
            from src.image_generator import save_image
            if save_image(img, self.image_path):
                self.push_client.push_image(self.image_path)
        except Exception as e:
            logger.error(f"화면 업데이트 실패: {e}")

    def setup_schedule(self):
        """스케줄 등록"""
        clock_interval   = self.config.get("refresh_interval_sec", 60)
        weather_interval = self.config.get("weather_interval_min", 10)

        # 시계 업데이트 (매 N초)
        schedule.every(clock_interval).seconds.do(self._update_display)

        # 날씨 업데이트 (매 N분)
        schedule.every(weather_interval).minutes.do(self._update_weather)

        logger.info(
            f"스케줄 등록: 시계 {clock_interval}초, 날씨 {weather_interval}분"
        )

        # 최초 실행
        self._update_weather()
        self._update_display()

    def run_forever(self):
        """블로킹 루프 (단독 실행 시)"""
        self._running = True
        self.setup_schedule()
        logger.info("스케줄러 시작 (Ctrl+C로 종료)")
        try:
            while self._running:
                schedule.run_pending()
                time.sleep(1)
        except KeyboardInterrupt:
            logger.info("스케줄러 종료")

    def start_background(self):
        """백그라운드 스레드로 실행 (Flask와 함께 사용)"""
        self.setup_schedule()
        self._running = True

        def _loop():
            while self._running:
                schedule.run_pending()
                time.sleep(1)

        self._thread = threading.Thread(target=_loop, daemon=True, name="scheduler")
        self._thread.start()
        logger.info("스케줄러 백그라운드 시작")

    def stop(self):
        self._running = False
        schedule.clear()
