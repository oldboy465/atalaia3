/**
 * Gestor de Filtros, Cópias e Purgas do Buffer Flash
 */
const FiltersManager = {
  currentRange: '15m',

  init() {
    this.bindRangeChips();
    this.bindCustomDates();
    this.bindBufferButtons();
    this.bindTrendToggle();
    this.bindSoundToggle();
  },

  bindRangeChips() {
    document.querySelectorAll('.filter-chip').forEach(btn => {
      btn.addEventListener('click', (e) => {
        document.querySelectorAll('.filter-chip').forEach(b => b.classList.remove('active'));
        e.target.classList.add('active');
        this.currentRange = e.target.getAttribute('data-range');
        this.applyFilter();
      });
    });
  },

  bindCustomDates() {
    const btn = document.getElementById('btnApplyCustomFilter');
    if (btn) {
      btn.addEventListener('click', () => {
        document.querySelectorAll('.filter-chip').forEach(b => b.classList.remove('active'));
        this.applyFilter(true);
      });
    }
  },

  bindTrendToggle() {
    const btn = document.getElementById('btnToggleTrends');
    const txt = document.getElementById('txtTrendToggle');
    if (btn) {
      btn.addEventListener('click', () => {
        const isShown = ChartsEngine.toggleTrendVisibility();
        if (txt) txt.innerText = isShown ? 'OCULTAR LINHAS DE TENDÊNCIA' : 'EXIBIR LINHAS DE TENDÊNCIA';
      });
    }
  },

  bindSoundToggle() {
    const btn = document.getElementById('btnToggleSound');
    if (btn) {
      btn.addEventListener('click', () => {
        Indicators.toggleSound();
      });
    }
  },

  bindBufferButtons() {
    const btnCopy = document.getElementById('btnCopyBuffer');
    if (btnCopy) {
      btnCopy.addEventListener('click', () => this.handleBufferSync('copy', btnCopy));
    }

    const btnExtract = document.getElementById('btnExtractBuffer');
    if (btnExtract) {
      btnExtract.addEventListener('click', () => {
        if (confirm('ATENÇÃO: A extração irá descarregar os dados e LIMPAR/PURGAR o LittleFS no ESP32. Confirmar?')) {
          this.handleBufferSync('extract', btnExtract);
        }
      });
    }
  },

  async handleBufferSync(mode, btnElement) {
    const originalText = btnElement.innerText;
    btnElement.disabled = true;
    btnElement.innerText = mode === 'copy' ? 'COPIANDO...' : 'EXTRAINDO & LIMPANDO...';

    try {
      const res = await fetch('/api/esp/sync', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mode })
      });
      const data = await res.json();
      if (data.status === 'success') {
        const msg = mode === 'copy'
          ? `Buffer copiado: ${data.synced_records} novas amostras inseridas localmente!`
          : `Extração concluída: ${data.synced_records} amostras salvas e memória Flash purgada!`;
        alert(msg);
        this.applyFilter();
      } else {
        alert(`Aviso: ${data.message || 'Sem resposta do microcontrolador.'}`);
      }
    } catch (err) {
      alert('Falha de ligação HTTP com o ESP32.');
    } finally {
      btnElement.disabled = false;
      btnElement.innerText = originalText;
    }
  },

  async applyFilter(isCustom = false) {
    let url = '/api/telemetry/data?';

    if (isCustom) {
      const start = document.getElementById('filterStart').value;
      const end = document.getElementById('filterEnd').value;
      if (start) url += `start=${encodeURIComponent(start.replace('T', ' '))}&`;
      if (end) url += `end=${encodeURIComponent(end.replace('T', ' '))}&`;
    } else {
      const now = new Date();
      let past = new Date(now);

      if (this.currentRange === '15m') past.setMinutes(now.getMinutes() - 15);
      else if (this.currentRange === '1h') past.setHours(now.getHours() - 1);
      else if (this.currentRange === '8h') past.setHours(now.getHours() - 8);
      else past = new Date(0);

      if (this.currentRange !== 'all') {
        const fmt = d => d.toISOString().slice(0, 19).replace('T', ' ');
        url += `start=${encodeURIComponent(fmt(past))}&`;
      }
    }

    try {
      const res = await fetch(url);
      const data = await res.json();
      ChartsEngine.populateHistory(data.records);

      if (data.summary && data.summary.latest) {
        Indicators.updateCards(data.summary.latest);
      }
      if (data.summary && data.summary.trends) {
        const elDT = document.getElementById('txtDeltaTemp');
        const elDG = document.getElementById('txtDeltaGas');
        const elAP = document.getElementById('txtAnomalyProb');
        if (elDT) elDT.innerText = `${data.summary.trends.temp_delta_per_min} °C/min`;
        if (elDG) elDG.innerText = `${data.summary.trends.gas_delta_per_min} ADC/min`;
        if (elAP) elAP.innerText = `${data.summary.trends.integrated_risk_score}`;
      }
    } catch (e) {
      console.error('[FILTROS] Erro ao obter dados filtrados:', e);
    }
  }
};