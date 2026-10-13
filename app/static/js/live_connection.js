/**
 * Conexão Bidirecional em Tempo Real (WebSocket + Interação de Atuadores)
 */
document.addEventListener("DOMContentLoaded", () => {
  ChartsEngine.initCharts();
  FiltersManager.init();
  FiltersManager.applyFilter();

  // Inicialização do WebSocket com Flask-SocketIO
  const socket = io({ credentials: true, transports: ['websocket', 'polling'] });
  const badge = document.getElementById('connStatusBadge');

  socket.on('connect', () => {
    badge.className = "badge-status badge-live";
    badge.innerText = "STREAM ATIVO (AO VIVO)";
  });

  socket.on('disconnect', () => {
    badge.className = "badge-status badge-connecting";
    badge.innerText = "RECONECTANDO...";
  });

  socket.on('live_telemetry', (sample) => {
    // 1. Atualização dos Cards e Medidores
    Indicators.updateCards(sample);
    // 2. Injeção animada nos gráficos
    ChartsEngine.pushLiveData(sample);
  });

  // Controle de Abertura e Fechamento do Modal
  const modal = document.getElementById('modalControl');
  const modalBox = modal.querySelector('.modal-box');
  const btnOpen = document.getElementById('btnOpenControl');
  const btnClose = document.getElementById('btnCloseModal');

  const openModal = () => {
    modal.classList.remove('hidden');
    setTimeout(() => modalBox.classList.add('modal-open'), 10);
  };

  const closeModal = () => {
    modalBox.classList.remove('modal-open');
    setTimeout(() => modal.classList.add('hidden'), 250);
  };

  btnOpen.addEventListener('click', openModal);
  btnClose.addEventListener('click', closeModal);
  modal.addEventListener('click', (e) => {
    if (e.target === modal) closeModal();
  });
});

/**
 * Disparo de Comandos de Atuadores Remotos
 */
async function triggerHardware(target) {
  const statusEl = document.getElementById('actuatorStatus');
  statusEl.innerText = `Disparando [${target}] no ESP32...`;
  try {
    const res = await fetch('/api/esp/actuator', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ target })
    });
    const d = await res.json();
    statusEl.innerText = d.status === 'ok' ? `Comando [${target}] executado com sucesso!` : `Falha ao acionar ${target}`;
  } catch (err) {
    statusEl.innerText = "Erro de conexão com o hardware.";
  }
}