# src/weather_api.py
# OpenWeatherMap API 연동 모듈

import requests
import json
import os
import time
import logging
from datetime import datetime

logger = logging.getLogger(__name__)

# 날씨 코드 → 이모지 매핑 (OpenWeatherMap 기준)
WEATHER_ICONS = {
    # 맑음
    "01d": "☀️", "01n": "🌙",
    # 구름 조금
    "02d": "⛅", "02n": "⛅",
    # 구름 많음
    "03d": "🌤", "03n": "🌤",
    # 흐림
    "04d": "☁️", "04n": "☁️",
    # 소나기
    "09d": "🌧", "09n": "🌧",
    # 비
    "10d": "🌦", "10n": "🌦",
    # 천둥번개
    "11d": "⛈", "11n": "⛈",
    # 눈
    "13d": "❄️", "13n": "❄️",
    # 안개/연무
    "50d": "🌫", "50n": "🌫",
}

WEATHER_DESC_KO = {
    "clear sky": "맑음",
    "few clouds": "구름 조금",
    "scattered clouds": "구름 많음",
    "broken clouds": "흐림",
    "shower rain": "소나기",
    "rain": "비",
    "thunderstorm": "천둥번개",
    "snow": "눈",
    "mist": "안개",
    "fog": "짙은 안개",
    "haze": "연무",
    "dust": "황사",
    "smoke": "연기",
    "drizzle": "이슬비",
    "overcast clouds": "흐림",
    "light rain": "가벼운 비",
    "moderate rain": "비",
    "heavy intensity rain": "강한 비",
    "light snow": "가벼운 눈",
    "heavy snow": "강한 눈",
}


class WeatherAPI:
    def __init__(self, config: dict):
        self.api_key = config.get("api_key", "")
        self.city = config.get("city", "Seoul")
        self.units = config.get("temp_unit", "metric")  # metric=섭씨
        self.cache_file = os.path.join(
            os.path.dirname(os.path.dirname(__file__)), "cache", "weather.json"
        )
        self.cache_ttl = config.get("weather_interval_min", 10) * 60  # 초 단위
        self._last_data = None

    def get_weather(self) -> dict:
        """날씨 데이터 반환 (캐시 우선)"""
        cached = self._load_cache()
        if cached:
            logger.debug("날씨 캐시 사용")
            return cached

        return self._fetch_weather()

    def _fetch_weather(self) -> dict:
        """OpenWeatherMap API 호출"""
        if not self.api_key:
            logger.warning("API Key 없음 — 더미 데이터 반환")
            return self._dummy_data()

        try:
            url = "https://api.openweathermap.org/data/2.5/weather"
            params = {
                "q": self.city,
                "appid": self.api_key,
                "units": self.units,
                "lang": "en",
            }
            resp = requests.get(url, params=params, timeout=10)
            resp.raise_for_status()
            raw = resp.json()

            data = self._parse(raw)
            self._save_cache(data)
            logger.info(f"날씨 업데이트: {data['temp']}°C, {data['desc_ko']}")
            return data

        except requests.exceptions.ConnectionError:
            logger.error("네트워크 오류 — 캐시 또는 더미 데이터 사용")
        except requests.exceptions.Timeout:
            logger.error("API 타임아웃")
        except requests.exceptions.HTTPError as e:
            logger.error(f"API 오류: {e}")
        except Exception as e:
            logger.error(f"날씨 조회 실패: {e}")

        # 실패 시 마지막 캐시 (만료되어도) 또는 더미 반환
        stale = self._load_cache(ignore_ttl=True)
        return stale if stale else self._dummy_data()

    def _parse(self, raw: dict) -> dict:
        """API 응답 파싱"""
        icon_code = raw["weather"][0]["icon"]
        desc_en = raw["weather"][0]["description"].lower()

        return {
            "city": raw.get("name", self.city),
            "temp": round(raw["main"]["temp"]),
            "feels_like": round(raw["main"]["feels_like"]),
            "temp_min": round(raw["main"]["temp_min"]),
            "temp_max": round(raw["main"]["temp_max"]),
            "humidity": raw["main"]["humidity"],
            "pressure": raw["main"]["pressure"],
            "wind_speed": round(raw.get("wind", {}).get("speed", 0), 1),
            "icon": WEATHER_ICONS.get(icon_code, "🌡"),
            "icon_code": icon_code,
            "desc_en": desc_en,
            "desc_ko": WEATHER_DESC_KO.get(desc_en, desc_en),
            "clouds": raw.get("clouds", {}).get("all", 0),
            "updated_at": time.time(),
        }

    def _dummy_data(self) -> dict:
        """API Key 없거나 오류 시 더미 데이터"""
        return {
            "city": self.city,
            "temp": 15,
            "feels_like": 13,
            "temp_min": 10,
            "temp_max": 18,
            "humidity": 65,
            "pressure": 1013,
            "wind_speed": 2.5,
            "icon": "☀️",
            "icon_code": "01d",
            "desc_en": "clear sky",
            "desc_ko": "맑음 (샘플)",
            "clouds": 5,
            "updated_at": time.time(),
        }

    def _load_cache(self, ignore_ttl=False) -> dict | None:
        try:
            if not os.path.exists(self.cache_file):
                return None
            with open(self.cache_file, "r") as f:
                data = json.load(f)
            age = time.time() - data.get("updated_at", 0)
            if ignore_ttl or age < self.cache_ttl:
                return data
        except Exception:
            pass
        return None

    def _save_cache(self, data: dict):
        try:
            os.makedirs(os.path.dirname(self.cache_file), exist_ok=True)
            with open(self.cache_file, "w") as f:
                json.dump(data, f, ensure_ascii=False)
        except Exception as e:
            logger.error(f"캐시 저장 실패: {e}")
