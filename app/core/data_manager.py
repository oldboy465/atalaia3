import time
from typing import Dict, Any, List, Optional
from app.core.esp_client import ESP32Client
from app.core.database import DatabaseManager
from app.core.binary_decoder import BinaryDecoder
from app.analytics.bioclimatic import BioclimaticEngine
from app.analytics.gas_metrics import GasMetricsEngine
from app.analytics.anomaly_engine import AnomalyEngine
from app.analytics.trend_analyzer import TrendAnalyzer

class DataManager:
    """
    Orquestrador Unificado do ATALAIA 3:
    - Vincula streaming ao vivo com persistência no banco SQLite
    - Sincroniza e decodifica o buffer de anel circular da memória Flash (LittleFS)
    - Enriquece todos os dados com os derivados bioclimáticos, gases, anomalias e tendências
    """

    def __init__(self, db: DatabaseManager, esp: ESP32Client):
        self.db = db
        self.esp = esp
        self.last_sync_time = 0

    def enrich_sample(self, raw_sample: Dict[str, Any]) -> Dict[str, Any]:
        temp = raw_sample.get("temperature", 0.0)
        hum = raw_sample.get("humidity", 0.0)
        mq = raw_sample.get("mq135_raw", 0)

        # Indicadores derivados
        bio = BioclimaticEngine.compute_all(temp, hum)
        gas = GasMetricsEngine.compute_all(mq)

        enriched = {**raw_sample, **bio, **gas}
        return enriched

    def process_live_sample(self, current_data: Dict[str, Any]) -> Dict[str, Any]:
        enriched = self.enrich_sample(current_data)
        self.db.insert_sample(enriched, source="LIVE")
        return enriched

    def sync_ring_buffer(self) -> int:
        raw_bin = self.esp.fetch_ring_buffer()
        if not raw_bin:
            return 0

        decoded_samples = BinaryDecoder.decode_buffer(raw_bin)
        if not decoded_samples:
            return 0

        enriched_samples = [self.enrich_sample(s) for s in decoded_samples]
        analyzed_samples = AnomalyEngine.evaluate_batch(enriched_samples)

        inserted_count = self.db.insert_batch(analyzed_samples, source="RING_BUFFER")
        self.last_sync_time = time.time()
        return inserted_count

    def get_dashboard_payload(self, start_date: Optional[str] = None, end_date: Optional[str] = None) -> Dict[str, Any]:
        history = self.db.query_filtered(start_date=start_date, end_date=end_date, limit=2000)
        
        # Enriquecimento com anomalias e tendências
        history_analyzed = AnomalyEngine.evaluate_batch(history)
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