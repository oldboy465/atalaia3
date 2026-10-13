from flask import Blueprint, request, jsonify
from app import data_manager, esp_client, db_manager

api_bp = Blueprint("api", __name__)

@api_bp.route("/telemetry/data", methods=["GET"])
def get_telemetry_data():
    start_date = request.args.get("start")
    end_date = request.args.get("end")
    payload = data_manager.get_dashboard_payload(start_date=start_date, end_date=end_date)
    return jsonify(payload)

@api_bp.route("/records/list", methods=["GET"])
def list_records_paginated():
    page = int(request.args.get("page", 1))
    size = int(request.args.get("size", 20))
    result = db_manager.query_table_paginated(page=page, page_size=size)
    return jsonify(result)

@api_bp.route("/records/delete", methods=["POST"])
def delete_records():
    payload = request.get_json(silent=True) or {}
    record_ids = payload.get("ids", [])
    if not record_ids:
        return jsonify({"status": "error", "message": "Nenhum ID fornecido"}), 400
    deleted_count = db_manager.delete_records_batch(record_ids)
    return jsonify({"status": "success", "deleted_count": deleted_count})

@api_bp.route("/records/purge_all", methods=["POST"])
def purge_all():
    total_deleted = db_manager.purge_all_records()
    return jsonify({"status": "success", "purged": total_deleted})

@api_bp.route("/esp/sync", methods=["POST"])
def sync_esp_buffer():
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