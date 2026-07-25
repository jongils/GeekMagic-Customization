import base64
import json
import os
import tempfile
import unittest
from unittest.mock import Mock, patch

from werkzeug.security import generate_password_hash

from src.console_image import run_command
from src.scheduler import WeatherClockScheduler
from src import web_config
from camera_server import camera_server


class WebAuthTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False)
        json.dump(
            {"web_auth": {"username": "admin", "password_hash": generate_password_hash("secret")}},
            self.tmp,
        )
        self.tmp.close()
        self.old_config = web_config.CONFIG_FILE
        web_config.CONFIG_FILE = self.tmp.name
        self.client = web_config.app.test_client()

    def tearDown(self):
        web_config.CONFIG_FILE = self.old_config
        os.unlink(self.tmp.name)

    def test_request_without_credentials_is_rejected(self):
        response = self.client.get("/status")
        self.assertEqual(response.status_code, 401)

    def test_request_with_credentials_is_allowed(self):
        token = base64.b64encode(b"admin:secret").decode()
        response = self.client.get("/status", headers={"Authorization": f"Basic {token}"})
        self.assertEqual(response.status_code, 200)

    def test_non_ascii_wrong_password_is_rejected_without_server_error(self):
        token = base64.b64encode("admin:잘못된암호".encode("utf-8")).decode()
        response = self.client.get("/status", headers={"Authorization": f"Basic {token}"})
        self.assertEqual(response.status_code, 401)

    def test_config_stored_password_is_never_returned_in_plaintext(self):
        with open(self.tmp.name) as f:
            stored = json.load(f)
        self.assertNotIn("password", stored["web_auth"])
        self.assertNotEqual(stored["web_auth"]["password_hash"], "secret")


class WebAuthMigrationTests(unittest.TestCase):
    """구버전 평문 password 설정을 자동으로 해시로 마이그레이션하는지 검증"""

    def setUp(self):
        self.tmp = tempfile.NamedTemporaryFile(mode="w", suffix=".json", delete=False)
        json.dump({"web_auth": {"username": "admin", "password": "legacy-secret"}}, self.tmp)
        self.tmp.close()
        self.old_config = web_config.CONFIG_FILE
        web_config.CONFIG_FILE = self.tmp.name
        self.client = web_config.app.test_client()

    def tearDown(self):
        web_config.CONFIG_FILE = self.old_config
        os.unlink(self.tmp.name)

    def test_legacy_plaintext_password_still_authenticates(self):
        token = base64.b64encode(b"admin:legacy-secret").decode()
        response = self.client.get("/status", headers={"Authorization": f"Basic {token}"})
        self.assertEqual(response.status_code, 200)

    def test_legacy_plaintext_password_is_rewritten_to_hash_on_disk(self):
        token = base64.b64encode(b"admin:legacy-secret").decode()
        self.client.get("/status", headers={"Authorization": f"Basic {token}"})

        with open(self.tmp.name) as f:
            stored = json.load(f)
        self.assertNotIn("password", stored["web_auth"])
        self.assertTrue(stored["web_auth"]["password_hash"])
        self.assertNotEqual(stored["web_auth"]["password_hash"], "legacy-secret")


class ConsoleCommandTests(unittest.TestCase):
    @patch("src.console_image.subprocess.run")
    def test_arbitrary_command_is_blocked_before_subprocess(self, subprocess_run):
        output = run_command("touch /tmp/should-not-exist")
        self.assertEqual(output, "[허용되지 않은 명령]")
        subprocess_run.assert_not_called()


class CameraAuthTests(unittest.TestCase):
    def setUp(self):
        self.old_token = camera_server.API_TOKEN
        camera_server.API_TOKEN = "camera-secret"
        self.client = camera_server.app.test_client()

    def tearDown(self):
        camera_server.API_TOKEN = self.old_token

    def test_missing_token_is_rejected(self):
        self.assertEqual(self.client.get("/health").status_code, 401)

    def test_valid_token_is_allowed(self):
        response = self.client.get("/health", headers={"X-API-Key": "camera-secret"})
        self.assertEqual(response.status_code, 200)


class WeatherPushTests(unittest.TestCase):
    @patch("src.image_generator.save_image", return_value=True)
    def test_cpu_restore_theme_does_not_block_weather_when_cpu_disabled(self, _save):
        push_client = Mock()
        push_client.is_online.return_value = True
        scheduler = WeatherClockScheduler(
            {
                "cpu_monitor": {"enabled": False, "restore_theme": 1},
                "camera": {"enabled": False},
                "slideshow": {"enabled": False},
                "night_mode": {"enabled": False},
            },
            Mock(),
            Mock(return_value=object()),
            push_client,
        )
        scheduler._weather_cache = {"temp": 20}

        scheduler._update_display()

        push_client.push_image.assert_called_once()


if __name__ == "__main__":
    unittest.main()
