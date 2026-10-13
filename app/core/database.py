import sqlite3
from typing import List, Dict, Any, Optional
from datetime import datetime

class DatabaseManager:
    def __init__(self, db_path: str = "atalaia3_history.db"):
        self.db_path = db_path
        self._init_db()

    def _get_connection(self) -> sqlite3.Connection:
        conn = sqlite3.connect(self.db_path, check_same_thread=False)
        conn.row_factory = sqlite3.Row
        return conn

    def _init_db(self) -> None:
        with self._get_connection() as conn:
            cursor = conn.cursor()
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS telemetry (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                    session_id INTEGER NOT NULL DEFAULT 1,
                    uptime_sec INTEGER NOT NULL DEFAULT 0,
                    temperature REAL NOT NULL DEFAULT 0.0,
                    humidity REAL NOT NULL DEFAULT 0.0,
                    mq135_raw INTEGER NOT NULL DEFAULT 0,
                    rain_detected INTEGER NOT NULL DEFAULT 0,
                    flame_detected INTEGER NOT NULL DEFAULT 0,
                    heat_index REAL,
                    dew_point REAL,
                    gas_ppm REAL,
                    is_anomaly INTEGER DEFAULT 0,
                    source TEXT DEFAULT 'LIVE',
                    UNIQUE(session_id, uptime_sec) ON CONFLICT IGNORE
                )
            """)
            cursor.execute("CREATE INDEX IF NOT EXISTS idx_telemetry_time ON telemetry(timestamp);")
            cursor.execute("CREATE INDEX IF NOT EXISTS idx_telemetry_session ON telemetry(session_id);")
            conn.commit()

    def insert_sample(self, sample: Dict[str, Any], source: str = "LIVE") -> bool:
        query = """
            INSERT INTO telemetry (
                timestamp, session_id, uptime_sec, temperature, humidity,
                mq135_raw, rain_detected, flame_detected, heat_index,
                dew_point, gas_ppm, is_anomaly, source
            ) VALUES (
                :timestamp, :session_id, :uptime_sec, :temperature, :humidity,
                :mq135_raw, :rain_detected, :flame_detected, :heat_index,
                :dew_point, :gas_ppm, :is_anomaly, :source
            )
        """
        data = {
            "timestamp": sample.get("timestamp", datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S")),
            "session_id": int(sample.get("session_id", 1)),
            "uptime_sec": int(sample.get("uptime_sec", 0)),
            "temperature": float(sample.get("temperature", 0.0)),
            "humidity": float(sample.get("humidity", 0.0)),
            "mq135_raw": int(sample.get("mq135_raw", 0)),
            "rain_detected": 1 if sample.get("rain_detected") else 0,
            "flame_detected": 1 if sample.get("flame_detected") else 0,
            "heat_index": sample.get("heat_index"),
            "dew_point": sample.get("dew_point"),
            "gas_ppm": sample.get("estimated_ppm") or sample.get("gas_ppm"),
            "is_anomaly": 1 if sample.get("is_anomaly") else 0,
            "source": source
        }
        with self._get_connection() as conn:
            try:
                cursor = conn.cursor()
                cursor.execute(query, data)
                conn.commit()
                return cursor.rowcount > 0
            except sqlite3.Error as err:
                print(f"[DB ERROR] Falha no insert: {err}")
                return False

    def insert_batch(self, samples: List[Dict[str, Any]], source: str = "SYNC") -> int:
        if not samples:
            return 0
        inserted = 0
        with self._get_connection() as conn:
            cursor = conn.cursor()
            for s in samples:
                data = (
                    s.get("timestamp", datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S")),
                    int(s.get("session_id", 1)),
                    int(s.get("uptime_sec", 0)),
                    float(s.get("temperature", 0.0)),
                    float(s.get("humidity", 0.0)),
                    int(s.get("mq135_raw", 0)),
                    1 if s.get("rain_detected") else 0,
                    1 if s.get("flame_detected") else 0,
                    s.get("heat_index"),
                    s.get("dew_point"),
                    s.get("estimated_ppm") or s.get("gas_ppm"),
                    1 if s.get("is_anomaly") else 0,
                    source
                )
                try:
                    cursor.execute("""
                        INSERT OR IGNORE INTO telemetry (
                            timestamp, session_id, uptime_sec, temperature, humidity,
                            mq135_raw, rain_detected, flame_detected, heat_index,
                            dew_point, gas_ppm, is_anomaly, source
                        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                    """, data)
                    if cursor.rowcount > 0:
                        inserted += 1
                except sqlite3.Error:
                    continue
            conn.commit()
        return inserted

    def query_filtered(self, start_date: Optional[str] = None, end_date: Optional[str] = None, limit: int = 5760) -> List[Dict[str, Any]]:
        sql = "SELECT * FROM telemetry WHERE 1=1"
        params: List[Any] = []
        if start_date:
            sql += " AND timestamp >= ?"
            params.append(start_date)
        if end_date:
            sql += " AND timestamp <= ?"
            params.append(end_date)
        sql += " ORDER BY timestamp ASC LIMIT ?"
        params.append(limit)

        with self._get_connection() as conn:
            cursor = conn.cursor()
            cursor.execute(sql, params)
            rows = cursor.fetchall()
            return [dict(row) for row in rows]