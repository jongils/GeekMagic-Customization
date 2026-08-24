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
        self._weather_cache       = None
        self._running             = False
        self._thread              = None
        self._cpu_thread          = None
        self._slideshow_thread    = None
        self._push_lock           = threading.Lock()  # Push 동시 실행 방지
        self._showing_cpu         = False             # CPU 표시 중 플래그
        self._showing_slideshow   = False             # 슬라이드쇼 표시 중 플래그
        self._console_thread      = None
        self._showing_console     = False             # 콘솔 표시 중 플래그
        self._camera_thread       = None
        self._showing_camera      = False             # 카메라 슬라이드쇼 표시 중 플래그
        self._temp_push_thread    = None              # 온도 전송 스레드

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

    def _restore_theme(self) -> int:
        """복원 테마 번호 반환 (0 = 날씨 화면 Push, 1~7 = 장치 내장 테마)"""
        return int(self.config.get("cpu_monitor", {}).get("restore_theme", 0))

    # ── 날씨 화면 Push ────────────────────────────────────────
    def _update_display(self):
        """날씨+시계 이미지 생성 → 장치 Push.
        슬라이드쇼·내장 테마 모드·CPU 표시 중에는 skip.
        """
        if self.config.get("camera", {}).get("enabled", False):
            logger.debug("카메라 슬라이드쇼 모드 — 날씨 Push 건너뜀")
            return
        if self.config.get("slideshow", {}).get("enabled", False):
            logger.debug("슬라이드쇼 모드 — 날씨 Push 건너뜀")
            return
        if (self.config.get("cpu_monitor", {}).get("enabled", False)
                and self._restore_theme() != 0):
            logger.debug("내장 테마 모드 — 날씨 Push 건너뜀")
            return
        if self._showing_cpu or self._showing_slideshow or self._showing_console or self._showing_camera:
            logger.debug("다른 콘텐츠 표시 중 — 날씨 Push 건너뜀")
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

        restore_theme == 0 : 날씨 화면(rest_sec) → CPU(show_sec) → 날씨 Push 복원
        restore_theme 1~7  : 내장 테마(rest_sec) → CPU(show_sec) → /set?theme=N 복원

        매 사이클 시작 시 self.config를 다시 읽어 웹 UI 변경 사항을 반영한다.
        """
        from src.cpu_image import generate_cpu_image
        from src.image_generator import save_image

        cpu_path = os.path.join(
            os.path.dirname(os.path.dirname(__file__)), "cache", "cpu_current.jpg"
        )

        # ── 최초 기동 시 설정 읽기 ──────────────────────────
        cfg           = self.config.get("cpu_monitor", {})
        show_sec      = max(int(cfg.get("show_sec",   10)), 1)
        rest_sec      = max(int(cfg.get("rest_sec",   50)), 5)
        restore_theme = int(cfg.get("restore_theme",   0))

        mode_label = f"내장 테마 {restore_theme}" if restore_theme else "날씨 화면"
        logger.info(
            f"CPU 모니터 스레드 시작 — 테마 {rest_sec}초 / CPU {show_sec}초 / 복원: {mode_label}"
        )

        # ── 내장 테마 모드: 시작 시 즉시 테마 전환 ────────────
        if restore_theme:
            self.push_client.set_theme(restore_theme)

        while self._running:
            # ── 비활성화 감지 시 스레드 자가 종료 ────────────
            if not self.config.get("cpu_monitor", {}).get("enabled", False):
                logger.info("CPU 모니터 비활성화 감지 — 스레드 종료")
                self._showing_cpu = False
                if restore_theme:
                    self.push_client.set_theme(restore_theme)
                else:
                    self._update_display()
                return

            # 슬라이드쇼가 활성화되어 있으면 CPU 모니터 양보
            if self.config.get("slideshow", {}).get("enabled", False):
                time.sleep(5)
                continue

            # ── 매 사이클 시작 시 config 재로드 ──────────────
            cfg       = self.config.get("cpu_monitor", {})
            show_sec  = max(int(cfg.get("show_sec",  10)), 1)
            rest_sec  = max(int(cfg.get("rest_sec",  50)), 5)
            new_theme = int(cfg.get("restore_theme",  0))

            # restore_theme 값이 변경된 경우 즉시 전환
            if new_theme != restore_theme:
                logger.info(f"복원 테마 변경 감지: {restore_theme} → {new_theme}")
                restore_theme = new_theme
                if restore_theme:
                    self.push_client.set_theme(restore_theme)

            logger.debug(f"사이클: 테마 {rest_sec}초 → CPU {show_sec}초")

            # ── 대기 기간 (내장 테마 or 날씨 화면 표시 중) ────
            for _ in range(rest_sec):
                if not self._running:
                    return
                if not self.config.get("cpu_monitor", {}).get("enabled", False):
                    break   # 다음 while 루프 최상단에서 종료 처리
                time.sleep(1)

            if not self._running:
                return

            # ── CPU 이미지 생성 및 Push ───────────────────────
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

            # ── CPU 표시 기간 대기 ────────────────────────────
            for _ in range(show_sec):
                if not self._running:
                    self._showing_cpu = False
                    return
                time.sleep(1)

            self._showing_cpu = False

            if not self._running:
                return

            # ── 복원 ──────────────────────────────────────────
            if restore_theme:
                # 내장 테마로 복원 (/set?theme=N, 이미지 업로드 없음)
                logger.info(f"CPU 종료 → 내장 테마 {restore_theme} 복원")
                self.push_client.set_theme(restore_theme)
            else:
                # 날씨 화면 복원 (weather_clock.jpg Push)
                logger.info("CPU 종료 → 날씨 화면 복원")
                self._update_display()

    # ── CPU 온도 전송 스레드 (192.168.219.122) ───────────────
    def _temp_push_loop(self):
        """Pi CPU 온도를 주기적으로 ESP8266(device_ip2)에 전송.

        config.temp_push.device_ip   : 대상 장치 IP
        config.temp_push.interval_sec: 전송 주기 (기본 30초)
        """
        from src.push_client import push_temp_to, get_cpu_temp

        cfg         = self.config.get("temp_push", {})
        device_ip2  = cfg.get("device_ip", "")
        interval    = max(int(cfg.get("interval_sec", 30)), 10)

        logger.info(f"온도 전송 스레드 시작 — 대상: {device_ip2}, 주기: {interval}초")

        while self._running:
            # 비활성화 감지
            if not self.config.get("temp_push", {}).get("enabled", False):
                logger.info("온도 전송 비활성화 감지 — 스레드 종료")
                return

            temp_c = get_cpu_temp()
            if temp_c is not None:
                ok = push_temp_to(device_ip2, temp_c)
                logger.debug(f"온도 전송 → {device_ip2}: {temp_c:.1f}°C ({'OK' if ok else 'FAIL'})")
            else:
                logger.warning("CPU 온도 읽기 실패")

            # interval 동안 1초씩 쪼개어 대기 (빠른 종료 감지)
            for _ in range(interval):
                if not self._running:
                    return
                time.sleep(1)

    # ── 슬라이드쇼 스레드 ────────────────────────────────────
    def _slideshow_loop(self):
        """로컬 폴더 슬라이드쇼 스레드.

        rest_sec == 0 : 사진 연속 재생 (photo→photo→...)
        rest_sec  > 0 : 내장 테마(rest_sec) → 사진(show_sec) → 반복
        """
        from src.slideshow import load_image_list, prepare_image

        cache_dir  = os.path.join(
            os.path.dirname(os.path.dirname(__file__)), "cache", "slideshow"
        )
        os.makedirs(cache_dir, exist_ok=True)
        slide_path = os.path.join(cache_dir, "current.jpg")

        logger.info("슬라이드쇼 스레드 시작")

        # 시작 시 초기 테마 설정
        cfg           = self.config.get("slideshow", {})
        restore_theme = int(cfg.get("restore_theme", 1))
        rest_sec      = int(cfg.get("rest_sec", 0))
        if restore_theme and rest_sec > 0:
            self.push_client.set_theme(restore_theme)

        while self._running:
            # 카메라 슬라이드쇼가 활성화되어 있으면 슬라이드쇼 양보
            if self.config.get("camera", {}).get("enabled", False):
                time.sleep(5)
                continue

            # ── 비활성화 감지 시 스레드 자가 종료 ────────────
            if not self.config.get("slideshow", {}).get("enabled", False):
                logger.info("슬라이드쇼 비활성화 감지 — 스레드 종료")
                self._showing_slideshow = False
                if restore_theme:
                    self.push_client.set_theme(restore_theme)
                else:
                    self._update_display()
                return

            # ── 매 순환 시 config 재로드 ──────────────────────
            cfg           = self.config.get("slideshow", {})
            folder        = cfg.get("folder", "/home/pi/Pictures")
            show_sec      = max(int(cfg.get("show_sec",  10)), 1)
            rest_sec      = max(int(cfg.get("rest_sec",   0)), 0)
            shuffle       = bool(cfg.get("shuffle", False))
            new_theme     = int(cfg.get("restore_theme",  1))

            if new_theme != restore_theme:
                logger.info(f"슬라이드쇼 복원 테마 변경: {restore_theme} → {new_theme}")
                restore_theme = new_theme

            # ── 이미지 목록 로드 ──────────────────────────────
            images = load_image_list(folder, shuffle)
            if not images:
                logger.warning(f"슬라이드쇼: 이미지 없음 ({folder}), 10초 후 재시도")
                for _ in range(10):
                    if not self._running:
                        return
                    time.sleep(1)
                continue

            logger.info(
                f"슬라이드쇼: {len(images)}장 / show {show_sec}초"
                + (f" / 테마 {rest_sec}초" if rest_sec else " / 연속재생")
            )

            # ── 사진 순환 ─────────────────────────────────────
            for img_path in images:
                if not self._running:
                    return
                if not self.config.get("slideshow", {}).get("enabled", False):
                    break

                if rest_sec > 0:
                    if restore_theme:
                        self.push_client.set_theme(restore_theme)
                    for _ in range(rest_sec):
                        if not self._running:
                            return
                        if not self.config.get("slideshow", {}).get("enabled", False):
                            break
                        time.sleep(1)

                if not self._running:
                    return

                if not prepare_image(img_path, slide_path):
                    continue

                self._showing_slideshow = True
                try:
                    with self._push_lock:
                        self.push_client.push_image(slide_path)
                    logger.info(
                        f"슬라이드쇼: {os.path.basename(img_path)} ({show_sec}초)"
                    )
                except Exception as e:
                    logger.error(f"슬라이드쇼 Push 실패: {e}")
                    self._showing_slideshow = False
                    continue

                for _ in range(show_sec):
                    if not self._running:
                        self._showing_slideshow = False
                        return
                    if not self.config.get("slideshow", {}).get("enabled", False):
                        self._showing_slideshow = False
                        break
                    time.sleep(1)

                self._showing_slideshow = False

    # ── 콘솔 출력 스레드 ─────────────────────────────────────
    def _console_loop(self):
        """명령어 출력을 240×240 이미지로 렌더링해 장치에 표시하는 스레드.

        rest_sec == 0 : 연속 표시 — refresh_sec마다 명령 재실행 후 이미지 갱신
        rest_sec  > 0 : 교대 표시 — 내장 테마(rest_sec) → 콘솔(show_sec) → 반복
        """
        from src.console_image import generate_console_image
        from src.image_generator import save_image

        cache_dir  = os.path.join(
            os.path.dirname(os.path.dirname(__file__)), "cache", "console"
        )
        os.makedirs(cache_dir, exist_ok=True)
        console_path = os.path.join(cache_dir, "current.jpg")

        logger.info("콘솔 표시 스레드 시작")

        restore_theme = int(self.config.get("console", {}).get("restore_theme", 1))

        while self._running:
            if not self.config.get("console", {}).get("enabled", False):
                logger.info("콘솔 표시 비활성화 감지 — 스레드 종료")
                self._showing_console = False
                if restore_theme:
                    self.push_client.set_theme(restore_theme)
                else:
                    self._update_display()
                return

            cfg           = self.config.get("console", {})
            command       = cfg.get("command", "").strip()
            label         = cfg.get("label", "").strip()
            refresh_sec   = max(int(cfg.get("refresh_sec",   5)),  1)
            show_sec      = max(int(cfg.get("show_sec",     30)),  1)
            rest_sec      = max(int(cfg.get("rest_sec",      0)),  0)
            restore_theme = int(cfg.get("restore_theme",     1))

            if not command:
                time.sleep(5)
                continue

            if rest_sec > 0:
                if restore_theme:
                    self.push_client.set_theme(restore_theme)
                for _ in range(rest_sec):
                    if not self._running:
                        return
                    if not self.config.get("console", {}).get("enabled", False):
                        break
                    time.sleep(1)

                if not self._running:
                    return

                try:
                    img = generate_console_image(command, label)
                    if save_image(img, console_path):
                        self._showing_console = True
                        with self._push_lock:
                            self.push_client.push_image(console_path)
                        logger.debug(f"콘솔 표시 ({show_sec}초): {command[:40]}")
                except Exception as e:
                    logger.error(f"콘솔 이미지 생성 실패: {e}")
                    self._showing_console = False
                    time.sleep(5)
                    continue

                for _ in range(show_sec):
                    if not self._running:
                        self._showing_console = False
                        return
                    if not self.config.get("console", {}).get("enabled", False):
                        self._showing_console = False
                        break
                    time.sleep(1)

                self._showing_console = False

            else:
                try:
                    img = generate_console_image(command, label)
                    if save_image(img, console_path):
                        self._showing_console = True
                        with self._push_lock:
                            self.push_client.push_image(console_path)
                        logger.debug(f"콘솔 갱신: {command[:40]}")
                except Exception as e:
                    logger.error(f"콘솔 이미지 생성 실패: {e}")

                for _ in range(refresh_sec):
                    if not self._running:
                        self._showing_console = False
                        return
                    if not self.config.get("console", {}).get("enabled", False):
                        self._showing_console = False
                        break
                    time.sleep(1)

    # ── 카메라 슬라이드쇼 스레드 ─────────────────────────────
    def _camera_loop(self):
        """촬영된 사진을 시간순으로 반복 표시하는 스레드."""
        from src.slideshow import prepare_image

        camera_dir = os.path.join(
            os.path.dirname(os.path.dirname(__file__)), "cache", "camera_feed"
        )
        os.makedirs(camera_dir, exist_ok=True)
        cam_path = os.path.join(camera_dir, "display_current.jpg")

        logger.info("카메라 슬라이드쇼 스레드 시작")
        restore_theme = int(self.config.get("camera", {}).get("restore_theme", 1))

        while self._running:
            if not self.config.get("camera", {}).get("enabled", False):
                logger.info("카메라 슬라이드쇼 비활성화 감지 — 스레드 종료")
                self._showing_camera = False
                if restore_theme:
                    self.push_client.set_theme(restore_theme)
                else:
                    self._update_display()
                return

            cfg           = self.config.get("camera", {})
            show_sec      = max(int(cfg.get("show_sec", 10)), 1)
            restore_theme = int(cfg.get("restore_theme", 1))

            try:
                images = sorted(
                    os.path.join(camera_dir, f)
                    for f in os.listdir(camera_dir)
                    if f.lower().endswith((".jpg", ".jpeg", ".png"))
                    and f != "display_current.jpg"
                )
            except Exception:
                images = []

            if not images:
                logger.debug("카메라 슬라이드쇼: 사진 없음 — 대기 중")
                for _ in range(5):
                    if not self._running:
                        return
                    time.sleep(1)
                continue

            logger.info(f"카메라 슬라이드쇼: {len(images)}장 / {show_sec}초씩")

            for img_path in images:
                if not self._running:
                    return
                if not self.config.get("camera", {}).get("enabled", False):
                    break

                if not prepare_image(img_path, cam_path):
                    continue

                self._showing_camera = True
                try:
                    with self._push_lock:
                        self.push_client.push_image(cam_path)
                    logger.info(f"카메라: {os.path.basename(img_path)} ({show_sec}초)")
                except Exception as e:
                    logger.error(f"카메라 슬라이드쇼 Push 실패: {e}")
                    self._showing_camera = False
                    continue

                for _ in range(show_sec):
                    if not self._running:
                        self._showing_camera = False
                        return
                    if not self.config.get("camera", {}).get("enabled", False):
                        self._showing_camera = False
                        break
                    time.sleep(1)

                self._showing_camera = False

    # ── 스케줄 등록 ───────────────────────────────────────────
    def setup_schedule(self):
        clock_interval   = self.config.get("refresh_interval_sec", 60)
        weather_interval = self.config.get("weather_interval_min", 10)

        schedule.every(clock_interval).seconds.do(self._update_display)
        schedule.every(weather_interval).minutes.do(self._update_weather)

        logger.info(
            f"스케줄 등록: 시계 {clock_interval}초, 날씨 {weather_interval}분"
        )

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

        # CPU 모니터 스레드
        if self.config.get("cpu_monitor", {}).get("enabled", False):
            self._cpu_thread = threading.Thread(
                target=self._cpu_monitor_loop, daemon=True, name="cpu-monitor"
            )
            self._cpu_thread.start()
        else:
            logger.info("CPU 모니터 비활성화 (config: cpu_monitor.enabled=false)")

        # 슬라이드쇼 스레드
        if self.config.get("slideshow", {}).get("enabled", False):
            self._slideshow_thread = threading.Thread(
                target=self._slideshow_loop, daemon=True, name="slideshow"
            )
            self._slideshow_thread.start()
        else:
            logger.info("슬라이드쇼 비활성화 (config: slideshow.enabled=false)")

        # 콘솔 표시 스레드
        if self.config.get("console", {}).get("enabled", False):
            self._console_thread = threading.Thread(
                target=self._console_loop, daemon=True, name="console"
            )
            self._console_thread.start()
        else:
            logger.info("콘솔 표시 비활성화 (config: console.enabled=false)")

        # 카메라 슬라이드쇼 스레드
        if self.config.get("camera", {}).get("enabled", False):
            self._camera_thread = threading.Thread(
                target=self._camera_loop, daemon=True, name="camera"
            )
            self._camera_thread.start()
        else:
            logger.info("카메라 슬라이드쇼 비활성화 (config: camera.enabled=false)")

        # 온도 전송 스레드 (device_ip2)
        if self.config.get("temp_push", {}).get("enabled", False):
            self._temp_push_thread = threading.Thread(
                target=self._temp_push_loop, daemon=True, name="temp-push"
            )
            self._temp_push_thread.start()
        else:
            logger.info("온도 전송 비활성화 (config: temp_push.enabled=false)")

    # ── 정지 ──────────────────────────────────────────────────
    def stop(self):
        self._running           = False
        self._showing_cpu       = False
        self._showing_slideshow = False
        self._showing_console   = False
        self._showing_camera    = False
        schedule.clear()
