import math
from typing import Dict, Any

class GasMetricsEngine:
    """
    Calibração e conversão do sensor de gás MQ-135 acoplado ao ADC de 12 bits do ESP32:
    - Conversão ADC (0 a 4095) para Tensão (V)
    - Cálculo da Resistência do Sensor (Rs) baseada no resistor de carga RL (10k)
    - Normalização com R0 base (em ar limpo)
    - Estimativa de concentração equivalente de CO2/Gases em PPM
    """
    VCC = 3.3
    RL_KOHM = 10.0
    ADC_MAX = 4095.0
    # R0 padrão para ar limpo (ajustável por autocalibração)
    R0_CLEAN_AIR = 76.63

    @classmethod
    def adc_to_voltage(cls, adc_val: int) -> float:
        clamped = max(1, min(int(cls.ADC_MAX), adc_val))
        return (clamped / cls.ADC_MAX) * cls.VCC

    @classmethod
    def calculate_rs(cls, adc_val: int) -> float:
        v_out = cls.adc_to_voltage(adc_val)
        if v_out >= cls.VCC:
            v_out = cls.VCC - 0.001
        # Divisor de tensão: Rs = RL * (Vcc - Vout) / Vout
        rs = cls.RL_KOHM * (cls.VCC - v_out) / max(0.001, v_out)
        return rs

    @classmethod
    def calculate_ppm_estimate(cls, adc_val: int, r0: float = R0_CLEAN_AIR) -> float:
        rs = cls.calculate_rs(adc_val)
        ratio = rs / max(0.001, r0)
        # Curva exponencial de sensibilidade do MQ-135: PPM = A * (Rs/R0)^B
        # Para curva média de CO2/gases gerais: A ~ 116.6020682, B ~ -2.769034857
        try:
            ppm = 116.602 * (ratio ** -2.769)
            return round(max(10.0, min(10000.0, ppm)), 2)
        except (ValueError, OverflowError):
            return 400.0

    @classmethod
    def compute_all(cls, adc_val: int, r0: float = R0_CLEAN_AIR) -> Dict[str, Any]:
        voltage = cls.adc_to_voltage(adc_val)
        rs = cls.calculate_rs(adc_val)
        ppm = cls.calculate_ppm_estimate(adc_val, r0)
        
        # Categorização de Qualidade do Ar
        if adc_val < 1000:
            quality = "EXCELENTE"
            level = "success"
        elif adc_val < 1800:
            quality = "MODERADA"
            level = "warning"
        elif adc_val < 2600:
            quality = "RUIM"
            level = "alert"
        else:
            quality = "CRÍTICA / PERIGOSA"
            level = "danger"

        return {
            "mq135_raw": adc_val,
            "voltage_v": round(voltage, 3),
            "sensor_rs_kohm": round(rs, 2),
            "estimated_ppm": ppm,
            "air_quality_text": quality,
            "air_quality_level": level
        }