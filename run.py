from gevent import monkey
# Patch de sockets, DNS e threads sem colidir com os finalizers do Python 3.12
monkey.patch_all(thread=False)

from app import create_app, socketio

app = create_app()

if __name__ == "__main__":
    print("\n" + "=" * 60)
    print(" [ATALAIA 3] Servidor Operacional Iniciado em Modo Real-Time")
    print(" Aceda ao painel: http://localhost:5000")
    print("=" * 60 + "\n")
    # use_reloader=False evita que o Windows crie subprocessos concorrentes travando gevent
    socketio.run(app, host="0.0.0.0", port=5000, debug=False, use_reloader=False)