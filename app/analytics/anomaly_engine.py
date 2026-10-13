import numpy as np
import pandas as pd
from typing import List, Dict, Any
from sklearn.ensemble import IsolationForest

class AnomalyEngine:
    """
    Detector multivariado de anomalias com conversões tipadas seguras para Pylance/MyPy.
    """

    @staticmethod
    def detect_zscore(series: pd.Series, threshold: float = 3.0) -> List[bool]:
        if len(series) < 5:
            return [False] * len(series)
        
        arr = series.to_numpy(dtype=float)
        std_val = float(np.nanstd(arr))
        if std_val == 0.0 or np.isnan(std_val):
            return [False] * len(series)
        
        mean_val = float(np.nanmean(arr))
        z_scores = np.abs((arr - mean_val) / std_val)
        flags: np.ndarray = z_scores > threshold
        return [bool(x) for x in flags]

    @classmethod
    def apply_bollinger_bands(cls, df: pd.DataFrame, col: str, window: int = 12, num_std: float = 2.0) -> pd.DataFrame:
        if col not in df.columns or len(df) < window:
            df[f'{col}_upper'] = df[col] if col in df.columns else 0.0
            df[f'{col}_lower'] = df[col] if col in df.columns else 0.0
            df[f'{col}_is_outlier'] = False
            return df

        roll_mean = df[col].rolling(window=window, min_periods=1).mean()
        roll_std = df[col].rolling(window=window, min_periods=1).std().fillna(0.0)

        df[f'{col}_upper'] = (roll_mean + (roll_std * num_std)).round(2)
        df[f'{col}_lower'] = (roll_mean - (roll_std * num_std)).round(2)
        
        arr_val = df[col].to_numpy(dtype=float)
        arr_up = df[f'{col}_upper'].to_numpy(dtype=float)
        arr_low = df[f'{col}_lower'].to_numpy(dtype=float)
        
        outliers: np.ndarray = (arr_val > arr_up) | (arr_val < arr_low)
        df[f'{col}_is_outlier'] = [bool(x) for x in outliers]
        return df

    @classmethod
    def evaluate_batch(cls, records: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
        if not records:
            return []

        if len(records) < 10:
            for r in records:
                r["is_anomaly"] = False
                r["anomaly_score"] = 0.0
            return records

        df = pd.DataFrame(records)

        # 1. Z-Scores
        z_temp = cls.detect_zscore(df['temperature'])
        z_hum = cls.detect_zscore(df['humidity'])
        z_mq = cls.detect_zscore(df['mq135_raw'])

        # 2. Isolation Forest
        features = ['temperature', 'humidity', 'mq135_raw']
        try:
            clf = IsolationForest(contamination=0.03, random_state=42, n_estimators=50)
            X = df[features].to_numpy(dtype=float)
            preds = clf.fit_predict(X)
            scores = -clf.decision_function(X)
            forest_flags = [bool(p == -1) for p in preds]
            score_list = [round(float(s), 3) for s in scores]
        except Exception:
            forest_flags = [False] * len(df)
            score_list = [0.0] * len(df)

        # 3. Consolidação
        output: List[Dict[str, Any]] = []
        for i, row in enumerate(records):
            is_anom = bool(z_temp[i] or z_hum[i] or z_mq[i] or forest_flags[i])
            item = dict(row)
            item["is_anomaly"] = is_anom
            item["anomaly_score"] = score_list[i]
            output.append(item)

        return output