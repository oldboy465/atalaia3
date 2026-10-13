from gevent import monkey
monkey.patch_all()

from app import create_app, socketio

app = create_app()

if __name__ == "__main__":
    print("\n" + "=" * 60)
    print(" [ATALAIA 3] Servidor Operacional Iniciado em Modo Real-Time")
    print(" Aceda ao painel: http://localhost:5000")
    print("=" * 60 + "\n")
    socketio.run(app, host="0.0.0.0", port=5000, debug=True)