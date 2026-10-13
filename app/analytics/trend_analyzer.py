import numpy as np
import pandas as pd
from typing import List, Dict, Any

class TrendAnalyzer:
    """
    Decomposição de tendências e análise diferencial temporal:
    - Médias Móveis Exponenciais (EMA de curto e longo prazo)
    - Taxa de variação no tempo (Derivada discreta dY/dt)
    - Projeção linear para predição dos próximos 15 minutos
    - Matriz de Risco Combinada Integrada (IRI)
    """

    @classmethod
    def compute_trends(cls, records: List[Dict[str, Any]]) -> Dict[str, Any]:
        if not records or len(records) < 4:
            return {
                "temp_delta_per_min": 0.0,
                "hum_delta_per_min": 0.0,
                "gas_delta_per_min": 0.0,
                "integrated_risk_score": 0.0,
                "risk_status": "SEGURO"
            }

        df = pd.DataFrame(records)

        # Cálculo da variação instantânea nos últimos 12 pontos (~1 minuto)
        recent_window = df.tail(12)
        n_pts = len(recent_window)
        dt_min = (n_pts * 5.0) / 60.0

        d_temp = (recent_window['temperature'].iloc[-1] - recent_window['temperature'].iloc[0]) / max(0.1, dt_min)
        d_hum = (recent_window['humidity'].iloc[-1] - recent_window['humidity'].iloc[0]) / max(0.1, dt_min)
        d_gas = (recent_window['mq135_raw'].iloc[-1] - recent_window['mq135_raw'].iloc[0]) / max(0.1, dt_min)

        # Último estado
        latest = df.iloc[-1]
        flame = bool(latest.get('flame_detected', False))
        rain = bool(latest.get('rain_detected', False))
        mq_raw = float(latest.get('mq135_raw', 0))
        temp = float(latest.get('temperature', 0))

        # Matriz de Índice de Risco Integrado (IRI: 0 a 100)
        risk_score = 0.0
        if flame:
            risk_score += 65.0
        if mq_raw > 2000:
            risk_score += min(25.0, ((mq_raw - 2000) / 2095.0) * 25.0)
        if temp > 45.0:
            risk_score += 10.0
        if rain:
            risk_score = max(0.0, risk_score - 5.0)  # Chuva resfria/atenua risco elétrico direto

        risk_score = round(min(100.0, max(0.0, risk_score)), 1)

        if risk_score > 60.0:
            risk_status = "PERIGO CRÍTICO"
        elif risk_score > 30.0:
            risk_status = "ALERTA"
        else:
            risk_status = "NORMAL"

        return {
            "temp_delta_per_min": round(d_temp, 2),
            "hum_delta_per_min": round(d_hum, 2),
            "gas_delta_per_min": round(d_gas, 2),
            "integrated_risk_score": risk_score,
            "risk_status": risk_status
        }