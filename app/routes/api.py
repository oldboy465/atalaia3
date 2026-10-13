from flask import Blueprint, request, jsonify
from app import data_manager, esp_client

api_bp = Blueprint("api", __name__)

@api_bp.route("/telemetry/data", methods=["GET"])
def get_telemetry_data():
    start_date = request.args.get("start")
    end_date = request.args.get("end")
    payload = data_manager.get_dashboard_payload(start_date=start_date, end_date=end_date)
    return jsonify(payload)

@api_bp.route("/esp/sync", methods=["POST"])
def sync_esp_buffer():
    """Suporta os modos de ingestão: 'copy' ou 'extract'."""
    data = request.get_json(silent=True) or {}
    mode = data.get("mode", "copy")
    should_purge = (mode == "extract")
    
    result = data_manager.sync_ring_buffer(purge_after=should_purge)
    return jsonify(result)

@api_bp.route("/esp/actuator", methods=["POST"])
def trigger_actuator():
    data = request.get_json(silent=True) or {}
    target = data.get("target", "")
    success = esp_client.trigger_actuator(target)
    return jsonify({"status": "ok" if success else "failed", "target": target})