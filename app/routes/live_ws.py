import gevent
from flask_socketio import emit
from app import socketio, data_manager, esp_client

live_worker_running = False

def background_live_poller():
    global live_worker_running
    print("[SOCKETIO] Background live poller ativo.")
    while live_worker_running:
        try:
            current_raw = esp_client.get_current_telemetry()
            if current_raw:
                # Enriquece imediatamente com todos os derivados termodinâmicos e emite
                processed = data_manager.process_live_sample(current_raw)
                socketio.emit("live_telemetry", processed)
        except Exception as err:
            print(f"[POLLER WARN] Oscilação momentânea: {err}")
        
        gevent.sleep(2.0)

@socketio.on("connect")
def handle_connect():
    global live_worker_running
    if not live_worker_running:
        live_worker_running = True
        socketio.start_background_task(background_live_poller)
    emit("connection_ack", {"status": "connected", "stream": "active"})

@socketio.on("disconnect")
def handle_disconnect():
    pass