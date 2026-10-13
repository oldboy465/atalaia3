import time
from datetime import datetime
from typing import Dict, Any, List, Optional
from app.core.esp_client import ESP32Client
from app.core.database import DatabaseManager
from app.core.binary_decoder import BinaryDecoder
from app.analytics.bioclimatic import BioclimaticEngine
from app.analytics.gas_metrics import GasMetricsEngine
from app.analytics.anomaly_engine import AnomalyEngine
from app.analytics.trend_analyzer import TrendAnalyzer

class DataManager:
    def __init__(self, db: DatabaseManager, esp: ESP32Client):
        self.db = db
        self.esp = esp
        self.last_sync_time = 0
        self._current_session_fallback = int(time.time())

    def enrich_sample(self, raw_sample: Dict[str, Any]) -> Dict[str, Any]:
        temp = float(raw_sample.get("temperature", 0.0))
        hum = float(raw_sample.get("humidity", 0.0))
        mq = int(raw_sample.get("mq135_raw", 0))

        sample_copy = dict(raw_sample)
        if "session_id" not in sample_copy or sample_copy["session_id"] is None:
            sample_copy["session_id"] = self._current_session_fallback
        if "uptime_sec" not in sample_copy or sample_copy["uptime_sec"] is None:
            sample_copy["uptime_sec"] = int(time.time()) % 1000000
        if "timestamp" not in sample_copy:
            sample_copy["timestamp"] = datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S")

        # Cálculo completo e rigoroso de todas as métricas termodinâmicas e do sensor de gás
        bio = BioclimaticEngine.compute_all(temp, hum)
        gas = GasMetricsEngine.compute_all(mq)

        # Fusão de todas as métricas no dicionário
        sample_copy.update(bio)
        sample_copy.update(gas)
        
        # Garante a injeção com nomes exatos para o front-end
        sample_copy["heat_index"] = bio.get("heat_index", temp)
        sample_copy["dew_point"] = bio.get("dew_point", 0.0)
        sample_copy["humidex"] = bio.get("humidex", temp)
        sample_copy["vpd_kpa"] = bio.get("vpd_kpa", 0.0)
        sample_copy["enthalpy_kj_kg"] = bio.get("enthalpy_kj_kg", 0.0)
        sample_copy["condensation_margin_c"] = bio.get("condensation_margin_c", 0.0)
        sample_copy["absolute_humidity_g_m3"] = bio.get("absolute_humidity_g_m3", 0.0)
        sample_copy["vapor_pressure_hpa"] = bio.get("vapor_pressure_hpa", 0.0)
        sample_copy["voltage_v"] = gas.get("voltage_v", 0.0)
        sample_copy["sensor_rs_kohm"] = gas.get("sensor_rs_kohm", 0.0)
        sample_copy["estimated_ppm"] = gas.get("estimated_ppm", 400.0)

        return sample_copy

    def process_live_sample(self, current_data: Dict[str, Any]) -> Dict[str, Any]:
        enriched = self.enrich_sample(current_data)
        self.db.insert_sample(enriched, source="LIVE")
        return enriched

    def sync_ring_buffer(self, purge_after: bool = False) -> Dict[str, Any]:
        raw_bin = self.esp.fetch_ring_buffer()
        if not raw_bin:
            return {"status": "error", "synced_records": 0, "message": "Buffer binário indisponível no ESP32."}

        decoded_samples = BinaryDecoder.decode_buffer(raw_bin)
        if not decoded_samples:
            return {"status": "warning", "synced_records": 0, "message": "Nenhuma amostra válida decodificada."}

        enriched_samples = [self.enrich_sample(s) for s in decoded_samples]
        analyzed_samples = AnomalyEngine.evaluate_batch(enriched_samples)

        inserted_count = self.db.insert_batch(analyzed_samples, source="RING_BUFFER")
        self.last_sync_time = time.time()

        purged = False
        if purge_after:
            purged = self.esp.purge_buffer()

        return {
            "status": "success",
            "synced_records": inserted_count,
            "purged": purged,
            "mode": "EXTRAIR_E_LIMPAR" if purge_after else "COPIAR"
        }

    def get_dashboard_payload(self, start_date: Optional[str] = None, end_date: Optional[str] = None) -> Dict[str, Any]:
        history = self.db.query_filtered(start_date=start_date, end_date=end_date, limit=2000)
        
        # Enriquecimento forçado de todo o histórico recuperado da base de dados
        history_enriched = [self.enrich_sample(r) for r in history]
        history_analyzed = AnomalyEngine.evaluate_batch(history_enriched)
        trends = TrendAnalyzer.compute_trends(history_analyzed)
        
        latest = history_analyzed[-1] if history_analyzed else {}

        return {
            "summary": {
                "total_records": len(history_analyzed),
                "latest": latest,
                "trends": trends
            },
            "records": history_analyzed
        }