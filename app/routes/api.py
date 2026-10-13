from flask import Blueprint, request, jsonify
from app import data_manager, esp_client

api_bp = Blueprint("api", __name__)

@api_bp.route("/telemetry/data", methods=["GET"])
def get_telemetry_data():
    """Retorna o histórico com filtros temporais, indicadores derivados e tendências."""
    start_date = request.args.get("start")
    end_date = request.args.get("end")
    payload = data_manager.get_dashboard_payload(start_date=start_date, end_date=end_date)
    return jsonify(payload)

@api_bp.route("/esp/sync", methods=["POST"])
def sync_esp_buffer():
    """Força o descarregamento do LittleFS Ring Buffer do ESP32 para o SQLite."""
    count = data_manager.sync_ring_buffer()
    return jsonify({
        "status": "success",
        "synced_records": count,
        "timestamp": data_manager.last_sync_time
    })

@api_bp.route("/esp/actuator", methods=["POST"])
def trigger_actuator():
    """Aciona sirenes, bips ou o LED de diagnóstico na placa remota."""
    data = request.get_json(silent=True) or {}
    target = data.get("target", "")
    success = esp_client.trigger_actuator(target)
    return jsonify({"status": "ok" if success else "failed", "target": target})