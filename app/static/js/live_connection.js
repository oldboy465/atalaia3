/**
 * Ligação Bidirecional em Tempo Real (WebSocket com Polling Fallback)
 */
document.addEventListener('DOMContentLoaded', () => {
  ChartsEngine.initCharts();
  FiltersManager.init();
  FiltersManager.applyFilter();

  const badge = document.getElementById('connStatusBadge');

  // Fallback Polling direto caso o WebSocket encontre resistência
  let fallbackPollingInterval = null;

  const startFallbackPolling = () => {
    if (fallbackPollingInterval) return;
    fallbackPollingInterval = setInterval(async () => {
      try {
        const res = await fetch('/api/telemetry/data?');
        const d = await res.json();
        if (d && d.summary && d.summary.latest) {
          Indicators.updateCards(d.summary.latest);
          ChartsEngine.pushLiveData(d.summary.latest);
          if (badge) {
            badge.className = 'badge-status badge-live';
            badge.innerText = 'STREAM ATIVO (POLLING)';
          }
        }
      } catch (err) {
        if (badge) {
          badge.className = 'badge-status badge-connecting';
          badge.innerText = 'OFFLINE (TENTANDO...)';
        }
      }
    }, 2500);
  };

  // Inicialização do Socket.IO
  if (typeof io !== 'undefined') {
    try {
      const socket = io({ transports: ['websocket', 'polling'], timeout: 4000 });

      socket.on('connect', () => {
        if (fallbackPollingInterval) {
          clearInterval(fallbackPollingInterval);
          fallbackPollingInterval = null;
        }
        if (badge) {
          badge.className = 'badge-status badge-live';
          badge.innerText = 'STREAM ATIVO (AO VIVO)';
        }
      });

      socket.on('disconnect', () => {
        if (badge) {
          badge.className = 'badge-status badge-connecting';
          badge.innerText = 'RECONECTANDO...';
        }
        startFallbackPolling();
      });

      socket.on('live_telemetry', (sample) => {
        Indicators.updateCards(sample);
        ChartsEngine.pushLiveData(sample);
      });
    } catch (err) {
      startFallbackPolling();
    }
  } else {
    startFallbackPolling();
  }

  // Controlo do Modal de Atuadores
  const modal = document.getElementById('modalControl');
  const btnOpen = document.getElementById('btnOpenControl');
  const btnClose = document.getElementById('btnCloseModal');

  if (btnOpen && modal) {
    btnOpen.addEventListener('click', () => {
      modal.style.display = 'flex';
    });
  }

  if (btnClose && modal) {
    btnClose.addEventListener('click', () => {
      modal.style.display = 'none';
    });
  }

  if (modal) {
    modal.addEventListener('click', (e) => {
      if (e.target === modal) modal.style.display = 'none';
    });
  }
});

/**
 * Disparo Remoto de Atuadores no ESP32
 */
async function triggerHardware(target) {
  const statusEl = document.getElementById('actuatorStatus');
  if (statusEl) statusEl.innerText = `A enviar comando [${target}]...`;
  try {
    const res = await fetch('/api/esp/actuator', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ target })
    });
    const d = await res.json();
    if (statusEl) {
      statusEl.innerText = d.status === 'ok'
        ? `Comando [${target}] executado com sucesso no ESP32!`
        : `Falha ao acionar ${target}`;
    }
  } catch (err) {
    if (statusEl) statusEl.innerText = 'Erro ao contactar o backend.';
  }
}