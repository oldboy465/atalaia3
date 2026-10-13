import requests
from requests.adapters import HTTPAdapter
from urllib3.util import Retry
from typing import Dict, Any, Optional, Tuple

class ESP32Client:
    """
    Cliente HTTP persistente com Session pooling blindado contra deadlocks do Gevent/Windows.
    """
    def __init__(self, primary_url: str = "http://atalaia2.local", fallback_url: str = "http://192.168.4.1"):
        self.primary_url = primary_url.rstrip('/')
        self.fallback_url = fallback_url.rstrip('/')
        self.active_base_url = self.primary_url

        # Sessão HTTP persistente para evitar chamadas contínuas de destruição de socket
        self.session = requests.Session()
        retries = Retry(total=1, backoff_factor=0.1, status_forcelist=[500, 502, 503, 504])
        adapter = HTTPAdapter(pool_connections=2, pool_maxsize=4, max_retries=retries)
        self.session.mount("http://", adapter)
        self.session.mount("https://", adapter)

    def _resolve_url(self, endpoint: str) -> Tuple[Optional[requests.Response], str]:
        endpoint = endpoint.lstrip('/')
        # Tenta a base ativa primeiro, depois o IP direto e por último o mDNS
        candidates = [self.active_base_url]
        for url in [self.fallback_url, self.primary_url]:
            if url not in candidates:
                candidates.append(url)

        for base in candidates:
            url = f"{base}/{endpoint}"
            try:
                # Timeout explícito de conexão (1.5s) e leitura (2.5s)
                res = self.session.get(url, timeout=(1.5, 2.5))
                if res.status_code == 200:
                    self.active_base_url = base
                    return res, url
            except (requests.RequestException, Exception):
                continue
        return None, ""

    def get_current_telemetry(self) -> Optional[Dict[str, Any]]:
        res, _ = self._resolve_url("api/current")
        if res is not None:
            try:
                data = res.json()
                if isinstance(data, dict):
                    return data
            except (ValueError, Exception):
                return None
        return None

    def fetch_ring_buffer(self) -> Optional[bytes]:
        res, _ = self._resolve_url("atalaia_buffer.bin")
        if res is not None and res.content and len(res.content) > 0:
            return res.content
        return None

    def purge_buffer(self) -> bool:
        for base in [self.active_base_url, self.fallback_url]:
            url = f"{base}/test?target=reset_buffer"
            try:
                res = self.session.post(url, timeout=(2.0, 3.0))
                if res.status_code == 200:
                    return True
            except (requests.RequestException, Exception):
                continue
        return False

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
                res = self.session.post(url, timeout=(2.0, 3.0))
                if res.status_code == 200:
                    return True
            except (requests.RequestException, Exception):
                continue
        return False