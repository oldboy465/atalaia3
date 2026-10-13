import requests
from typing import Dict, Any, Optional, Tuple

class ESP32Client:
    def __init__(self, primary_url: str = "http://atalaia2.local", fallback_url: str = "http://192.168.4.1", timeout: float = 3.0):
        self.primary_url = primary_url.rstrip('/')
        self.fallback_url = fallback_url.rstrip('/')
        self.timeout = timeout
        self.active_base_url = self.primary_url

    def _resolve_url(self, endpoint: str) -> Tuple[Optional[requests.Response], str]:
        endpoint = endpoint.lstrip('/')
        for base in [self.active_base_url, self.fallback_url, self.primary_url]:
            url = f"{base}/{endpoint}"
            try:
                res = requests.get(url, timeout=self.timeout)
                if res.status_code == 200:
                    self.active_base_url = base
                    return res, url
            except requests.RequestException:
                continue
        return None, ""

    def get_current_telemetry(self) -> Optional[Dict[str, Any]]:
        res, _ = self._resolve_url("api/current")
        if res:
            try:
                return res.json()
            except ValueError:
                return None
        return None

    def fetch_ring_buffer(self) -> Optional[bytes]:
        res, _ = self._resolve_url("atalaia_buffer.bin")
        if res and len(res.content) > 0:
            return res.content
        return None

    def trigger_actuator(self, target: str) -> bool:
        target_map = {
            "buzzer5v": "buzzer5v",
            "siren": "buzzer3v_fire",
            "melody": "buzzer3v_rain",
            "toggle_led": "led_toggle",
            "auto_led": "led_auto"
        }
        cmd = target_map.get(target, target)
        for base in [self.active_base_url, self.fallback_url]:
            url = f"{base}/test?target={cmd}"
            try:
                res = requests.post(url, timeout=self.timeout)
                if res.status_code == 200:
                    return True
            except requests.RequestException:
                continue
        return False