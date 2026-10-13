import os
from dotenv import load_dotenv

load_dotenv()

class Config:
    SECRET_KEY = os.getenv("SECRET_KEY", "atalaia3_super_secret_matrix_key_2026")
    ESP32_HOST = os.getenv("ESP32_HOST", "http://atalaia2.local")
    ESP32_FALLBACK_IP = os.getenv("ESP32_FALLBACK_IP", "http://192.168.4.1")
    DATABASE_PATH = os.getenv("DATABASE_PATH", "atalaia3_history.db")
    SAMPLE_RATE_SEC = int(os.getenv("SAMPLE_RATE_SEC", 5))
    RING_BUFFER_SIZE = 5760
    PACKET_SIZE_BYTES = 24