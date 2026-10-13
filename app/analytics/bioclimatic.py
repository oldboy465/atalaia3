import math
from typing import Dict, Any

class BioclimaticEngine:
    """
    Cálculo de métricas psicrométricas e bioclimáticas:
    - Índice de Calor (Heat Index - Rothfusz / Steadman)
    - Ponto de Orvalho (Dew Point - Fórmula de Magnus-Tetens)
    - Pressão de Vapor de Saturação e Real (hPa)
    - Umidade Absoluta (g/m³)
    """

    @staticmethod
    def calculate_dew_point(temp_c: float, hum_pct: float) -> float:
        if hum_pct <= 0:
            return -50.0
        a = 17.27
        b = 237.7
        alpha = ((a * temp_c) / (b + temp_c)) + math.log(hum_pct / 100.0)
        dew_point = (b * alpha) / (a - alpha)
        return round(dew_point, 2)

    @staticmethod
    def calculate_heat_index(temp_c: float, hum_pct: float) -> float:
        # Converter para Fahrenheit para aplicação da regressão NOAA / Rothfusz
        tf = (temp_c * 9.0 / 5.0) + 32.0
        
        # Equação simplificada inicial
        hi_simple = 0.5 * (tf + 61.0 + ((tf - 68.0) * 1.2) + (hum_pct * 0.094))
        
        if (hi_simple + tf) / 2.0 < 80.0:
            hi_f = hi_simple
        else:
            # Equação polinomial completa de Rothfusz
            c1 = -42.379
            c2 = 2.04901523
            c3 = 10.14333127
            c4 = -0.22475541
            c5 = -0.00683783
            c6 = -0.05481717
            c7 = 0.00122874
            c8 = 0.00085282
            c9 = -0.00000199
            
            hi_f = (c1 + (c2 * tf) + (c3 * hum_pct) + (c4 * tf * hum_pct) +
                    (c5 * tf**2) + (c6 * hum_pct**2) + (c7 * (tf**2) * hum_pct) +
                    (c8 * tf * (hum_pct**2)) + (c9 * (tf**2) * (hum_pct**2)))
            
            # Ajustes finos de borda
            if hum_pct < 13.0 and 80.0 <= tf <= 112.0:
                adjustment = ((13.0 - hum_pct) / 4.0) * math.sqrt((17.0 - abs(tf - 95.0)) / 17.0)
                hi_f -= adjustment
            elif hum_pct > 85.0 and 80.0 <= tf <= 87.0:
                adjustment = ((hum_pct - 85.0) / 10.0) * ((87.0 - tf) / 5.0)
                hi_f += adjustment
                
        hi_c = (hi_f - 32.0) * 5.0 / 9.0
        return round(hi_c, 2)

    @staticmethod
    def calculate_vapor_and_humidity(temp_c: float, hum_pct: float) -> Dict[str, float]:
        # Pressão de vapor de saturação (hPa)
        es = 6.112 * math.exp((17.67 * temp_c) / (temp_c + 243.5))
        # Pressão de vapor atual
        ea = es * (hum_pct / 100.0)
        # Umidade absoluta em g/m³: (ea * 100 * 2.16679) / (temp_c + 273.15)
        absolute_humidity = (ea * 216.7) / (temp_c + 273.15)
        
        return {
            "sat_vapor_pressure_hpa": round(es, 2),
            "vapor_pressure_hpa": round(ea, 2),
            "absolute_humidity_g_m3": round(absolute_humidity, 2)
        }

    @classmethod
    def compute_all(cls, temp_c: float, hum_pct: float) -> Dict[str, float]:
        res = cls.calculate_vapor_and_humidity(temp_c, hum_pct)
        res["heat_index"] = cls.calculate_heat_index(temp_c, hum_pct)
        res["dew_point"] = cls.calculate_dew_point(temp_c, hum_pct)
        return res