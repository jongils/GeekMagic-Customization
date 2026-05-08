# src/scheduler.py
# 주기적 이미지 생성 및 Push 스케줄러 (CPU 모니터 통합)

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
        self.gen_fn       = image_gen_fn
        self.push_client  = push_client
        self.image_path   = os.path.join(
            os.path.dirname(os.path.dirname(__file__)), "cache", "current.jpg"
        )
        self._weather_cache = None
        self._running     = False
        self._thread      = None
        self._cpu_thread  = None
        self._push_lock   = threading.Lock()   # 날씨/CPU 동시 Push 방지
        self._showing_cpu = False              # CPU 표시 중 플래그

    # ── 야간 모드 ─────────────────────────────────────────────
    def _is_night_mode(self) -> bool:
        nm = self.config.get("night_mode", {})
        if not nm.get("enabled", False):
            return False
        try:
            now   = datetime.now(KST).strftime("%H:%M")
            start = nm.get("start", "23:00")
            end   = nm.get("end",   "07:00")
            if start > end:
                return now >= start or now < end
            return start <= now < end
        except Exception:
            return False

    # ── 날씨 갱신 ─────────────────────────────────────────────
    def _update_weather(self):
        logger.info("날씨 데이터 갱신 중...")
        self._weather_cache = self.weather_api.get_weather()

    # ── 날씨 화면 Push ────────────────────────────────────────
    def _update_display(self):
        """날씨+시계 이미지 생성 → 장치 Push (CPU 표시 중엔 건너뜀)"""
        if self._showing_cpu:
            logger.debug("CPU 모니터 표시 중 — 날씨 Push 건너뜀")
            return
        if self._is_night_mode():
            logger.debug("야간 모드 — Push 건너뜀")
            return
        if not self.push_client.is_online():
            logger.warning("장치 오프라인 — Push 건너뜀")
            return
        if self._weather_cache is None:
            self._update_weather()

        with self._push_lock:
            try:
                img = self.gen_fn(self._weather_cache, self.config)
                from src.image_generator import save_image
                if save_image(img, self.image_path):
                    self.push_client.push_image(self.image_path)
            except Exception as e:
                logger.error(f"날씨 화면 업데이트 실패: {e}")

    # ── CPU 모니터 스레드 ─────────────────────────────────────
    def _cpu_monitor_loop(self):
        """
        CPU 모니터 주기 실행 스레드
        날씨 화면(rest_sec) → CPU 화면(show_sec) → 날씨 복원 → 반복
        """
        from src.cpu_image import generate_cpu_image
        from src.image_generator import save_image

        cfg          = self.config.get("cpu_monitor", {})
        show_sec     = max(int(cfg.get("show_sec",    10)), 1)
        interval_sec = max(int(cfg.get("interval_sec", 60)), show_sec + 5)
        rest_sec     = interval_sec - show_sec

        cpu_path = os.path.join(
            os.path.dirname(os.path.dirname(__file__)), "cache", "cpu_current.jpg"
        )
        logger.info(
            f"CPU 모니터 스레드 시작 — 표시 {show_sec}초 / 주기 {interval_sec}초"
        )

        while self._running:
            # ── 날씨 표시 기간 대기 ───────────────────────
            for _ in range(rest_sec):
                if not self._running:
                    return
                time.sleep(1)

            if not self._running:
                return

            # ── CPU 이미지 생성 및 Push ───────────────────
            self._showing_cpu = True
            try:
                with self._push_lock:
                    img = generate_cpu_image()
                    if save_image(img, cpu_path):
                        self.push_client.push_image(cpu_path)
                        logger.info(f"CPU 모니터 표시 ({show_sec}초)")
            except Exception as e:
                logger.error(f"CPU 모니터 Push 실패: {e}")
                self._showing_cpu = False
                continue

            # ── CPU 표시 기간 대기 ────────────────────────
            for _ in range(show_sec):
                if not self._running:
                    self._showing_cpu = False
                    return
                time.sleep(1)

            self._showing_cpu = False

            if not self._running:
                return

            # ── 날씨 화면 즉시 복원 ───────────────────────
            logger.info("CPU 모니터 종료 → 날씨 화면 복원")
            self._update_display()

    # ── 스케줄 등록 ───────────────────────────────────────────
    def setup_schedule(self):
        clock_interval   = self.config.get("refresh_interval_sec", 60)
        weather_interval = self.config.get("weather_interval_min", 10)

        schedule.every(clock_interval).seconds.do(self._update_display)
        schedule.every(weather_interval).minutes.do(self._update_weather)

        logger.info(
            f"스케줄 등록: 시계 {clock_interval}초, 날씨 {weather_interval}분"
        )

        # 최초 실행
        self._update_weather()
        self._update_display()

    # ── 단독 블로킹 실행 ──────────────────────────────────────
    def run_forever(self):
        self._running = True
        self.setup_schedule()
        logger.info("스케줄러 시작 (Ctrl+C로 종료)")
        try:
            while self._running:
                schedule.run_pending()
                time.sleep(1)
        except KeyboardInterrupt:
            logger.info("스케줄러 종료")

    # ── 백그라운드 실행 (Flask 병행) ──────────────────────────
    def start_background(self):
        self.setup_schedule()
        self._running = True

        def _loop():
            while self._running:
                schedule.run_pending()
                time.sleep(1)

        self._thread = threading.Thread(target=_loop, daemon=True, name="scheduler")
        self._thread.start()
        logger.info("스케줄러 백그라운드 시작")

        # CPU 모니터 스레드 (config에서 활성화된 경우)
        if self.config.get("cpu_monitor", {}).get("enabled", False):
            self._cpu_thread = threading.Thread(
                target=self._cpu_monitor_loop, daemon=True, name="cpu-monitor"
            )
            self._cpu_thread.start()
        else:
            logger.info("CPU 모니터 비활성화 (config: cpu_monitor.enabled=false)")

    # ── 정지 ──────────────────────────────────────────────────
    def stop(self):
        self._running = False
        self._showing_cpu = False
        schedule.clear()
