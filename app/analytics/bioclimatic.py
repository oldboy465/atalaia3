import math
from typing import Dict, Any

class BioclimaticEngine:
    """
    Motor psicrométrico e bioclimático avançado:
    - Ponto de Orvalho (Magnus-Tetens)
    - Sensação Térmica / Heat Index (NOAA / Rothfusz)
    - Humidex Canadiano
    - Défice de Pressão de Vapor (VPD em kPa)
    - Entalpia Específica do ar úmido (kJ/kg)
    - Margem de Condensação Térmica (°C)
    """

    @staticmethod
    def calculate_dew_point(temp_c: float, hum_pct: float) -> float:
        if hum_pct <= 0:
            return -40.0
        a = 17.27
        b = 237.7
        alpha = ((a * temp_c) / (b + temp_c)) + math.log(max(0.01, hum_pct) / 100.0)
        dew_point = (b * alpha) / (a - alpha)
        return round(dew_point, 2)

    @staticmethod
    def calculate_heat_index(temp_c: float, hum_pct: float) -> float:
        tf = (temp_c * 9.0 / 5.0) + 32.0
        hi_simple = 0.5 * (tf + 61.0 + ((tf - 68.0) * 1.2) + (hum_pct * 0.094))
        
        if (hi_simple + tf) / 2.0 < 80.0:
            hi_f = hi_simple
        else:
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
                    (c5 * (tf**2)) + (c6 * (hum_pct**2)) + (c7 * (tf**2) * hum_pct) +
                    (c8 * tf * (hum_pct**2)) + (c9 * (tf**2) * (hum_pct**2)))
            
            if hum_pct < 13.0 and 80.0 <= tf <= 112.0:
                hi_f -= ((13.0 - hum_pct) / 4.0) * math.sqrt(max(0.0, (17.0 - abs(tf - 95.0)) / 17.0))
            elif hum_pct > 85.0 and 80.0 <= tf <= 87.0:
                hi_f += ((hum_pct - 85.0) / 10.0) * ((87.0 - tf) / 5.0)
                
        hi_c = (hi_f - 32.0) * 5.0 / 9.0
        return round(hi_c, 2)

    @staticmethod
    def calculate_humidex(temp_c: float, dew_point_c: float) -> float:
        # Fórmula do índice de conforto térmico canadiano
        e = 6.11 * math.exp(5417.7530 * ((1.0 / 273.16) - (1.0 / (273.15 + dew_point_c))))
        humidex = temp_c + (5.0 / 9.0) * (e - 10.0)
        return round(humidex, 2)

    @staticmethod
    def calculate_vpd(temp_c: float, hum_pct: float) -> float:
        # Pressão de vapor de saturação (VPsat em kPa)
        vp_sat = 0.61078 * math.exp((17.27 * temp_c) / (temp_c + 237.3))
        # Pressão de vapor real (VPact em kPa)
        vp_act = vp_sat * (hum_pct / 100.0)
        vpd = vp_sat - vp_act
        return round(max(0.0, vpd), 3)

    @staticmethod
    def calculate_enthalpy(temp_c: float, hum_pct: float) -> float:
        # Relação de mistura (w em kg vapor / kg ar seco)
        p_atm = 1013.25 # hPa
        es = 6.112 * math.exp((17.67 * temp_c) / (temp_c + 243.5))
        ea = es * (hum_pct / 100.0)
        w = 0.622 * (ea / max(1.0, (p_atm - ea)))
        # Entalpia h = 1.006*T + w*(2501 + 1.86*T)
        h = (1.006 * temp_c) + (w * (2501.0 + (1.86 * temp_c)))
        return round(h, 2)

    @classmethod
    def compute_all(cls, temp_c: float, hum_pct: float) -> Dict[str, float]:
        dp = cls.calculate_dew_point(temp_c, hum_pct)
        hi = cls.calculate_heat_index(temp_c, hum_pct)
        hmdx = cls.calculate_humidex(temp_c, dp)
        vpd = cls.calculate_vpd(temp_c, hum_pct)
        enthalpy = cls.calculate_enthalpy(temp_c, hum_pct)
        
        # Margem de condensação: quanto menor, maior a formação de orvalho/gotículas
        condensation_margin = round(temp_c - dp, 2)

        es = 6.112 * math.exp((17.67 * temp_c) / (temp_c + 243.5))
        ea = es * (hum_pct / 100.0)
        abs_hum = (ea * 216.7) / (temp_c + 273.15)

        return {
            "heat_index": hi,
            "dew_point": dp,
            "humidex": hmdx,
            "vpd_kpa": vpd,
            "enthalpy_kj_kg": enthalpy,
            "condensation_margin_c": condensation_margin,
            "sat_vapor_pressure_hpa": round(es, 2),
            "vapor_pressure_hpa": round(ea, 2),
            "absolute_humidity_g_m3": round(abs_hum, 2)
        }