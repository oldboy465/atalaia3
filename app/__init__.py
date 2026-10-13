# type: ignore
import sys
from flask import Flask
from flask_socketio import SocketIO
from config import Config
from app.core.database import DatabaseManager
from app.core.esp_client import ESP32Client
from app.core.data_manager import DataManager

# Instanciação dos serviços centrais
socketio = SocketIO(cors_allowed_origins="*", async_mode="gevent")
db_manager = DatabaseManager(Config.DATABASE_PATH)
esp_client = ESP32Client(Config.ESP32_HOST, Config.ESP32_FALLBACK_IP)
data_manager = DataManager(db_manager, esp_client)

def create_app():
    application = Flask(__name__)
    application.config.from_object(Config)

    # Registro tardio dos Blueprints para isolar ciclo de importações
    from app.routes.views import views_bp
    from app.routes.api import api_bp
    import app.routes.live_ws

    application.register_blueprint(views_bp)
    application.register_blueprint(api_bp, url_prefix="/api")

    socketio.init_app(application)
    return application