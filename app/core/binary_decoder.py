import struct
from typing import Dict, Any, List

class BinaryDecoder:
    """
    Decodificador do struct SensorData packed (24 bytes) do firmware ESP32:
    - float    temperature;    (4 bytes, <f)
    - float    humidity;       (4 bytes, <f)
    - uint32_t uptime_sec;     (4 bytes, <I)
    - uint32_t session_id;     (4 bytes, <I)
    - int32_t  mq135_raw;      (4 bytes, <i)
    - uint8_t  rain_detected;  (1 byte,  <B)
    - uint8_t  flame_detected; (1 byte,  <B)
    - uint8_t  padding[2];     (2 bytes, <2s)
    Formato Little-Endian struct: '<ffIIiBB2s' (total = 24 bytes)
    """
    STRUCT_FORMAT = '<ffIIiBB2s'
    RECORD_SIZE = 24

    @classmethod
    def decode_sample(cls, raw_bytes: bytes) -> Dict[str, Any]:
        if len(raw_bytes) != cls.RECORD_SIZE:
            raise ValueError(f"Tamanho do buffer inválido: {len(raw_bytes)} bytes (esperado {cls.RECORD_SIZE})")

        temp, hum, uptime, session_id, mq_raw, rain, flame, _ = struct.unpack(cls.STRUCT_FORMAT, raw_bytes)

        return {
            "temperature": round(float(temp), 2),
            "humidity": round(float(hum), 2),
            "uptime_sec": int(uptime),
            "session_id": int(session_id),
            "mq135_raw": int(mq_raw),
            "rain_detected": bool(rain == 1),
            "flame_detected": bool(flame == 1)
        }

    @classmethod
    def decode_buffer(cls, raw_stream: bytes) -> List[Dict[str, Any]]:
        samples = []
        total_len = len(raw_stream)
        num_records = total_len // cls.RECORD_SIZE

        for i in range(num_records):
            offset = i * cls.RECORD_SIZE
            chunk = raw_stream[offset:offset + cls.RECORD_SIZE]
            try:
                sample = cls.decode_sample(chunk)
                # Filtra blocos não inicializados (zeros completos ou ruído nulo)
                if sample["session_id"] != 0 or sample["uptime_sec"] != 0:
                    samples.append(sample)
            except Exception:
                continue

        return samples