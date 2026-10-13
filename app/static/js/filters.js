/**
 * Gerenciamento Dinâmico de Filtros, Intervalos e Sincronização
 */
const FiltersManager = {
  currentRange: '15m',

  init() {
    this.bindRangeChips();
    this.bindCustomDates();
    this.bindBufferSync();
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

  bindBufferSync() {
    const btn = document.getElementById('btnSyncBuffer');
    if (btn) {
      btn.addEventListener('click', async () => {
        btn.disabled = true;
        btn.innerText = "Sincronizando Buffer...";
        try {
          const res = await fetch('/api/esp/sync', { method: 'POST' });
          const data = await res.json();
          alert(`Buffer Flash baixado com sucesso: ${data.synced_records} amostras integradas!`);
          this.applyFilter();
        } catch (err) {
          alert("Falha ao sincronizar com o ESP32. Verifique o Wi-Fi.");
        } finally {
          btn.disabled = false;
          btn.innerText = "Sincronizar Buffer Flash";
        }
      });
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
      else past = new Date(0); // All

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
        document.getElementById('txtDeltaTemp').innerText = `${data.summary.trends.temp_delta_per_min} °C/min`;
        document.getElementById('txtDeltaGas').innerText = `${data.summary.trends.gas_delta_per_min} ADC/min`;
      }
    } catch (e) {
      console.error("Erro ao aplicar filtros:", e);
    }
  }
};