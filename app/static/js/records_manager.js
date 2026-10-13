/**
 * Gerenciador de Auditoria e Exclusão em Lote do Histórico de Telemetria
 */
const RecordsManager = {
  currentPage: 1,
  pageSize: 20,
  totalRecords: 0,
  selectedIds: new Set(),

  init() {
    this.bindEvents();
    this.loadRecords();
  },

  bindEvents() {
    // 1. Mudança de quantidade por página
    const selSize = document.getElementById('selectPageSize');
    if (selSize) {
      selSize.addEventListener('change', (e) => {
        this.pageSize = parseInt(e.target.value);
        this.currentPage = 1;
        this.selectedIds.clear();
        this.updateSelectedUI();
        this.loadRecords();
      });
    }

    // 2. Paginação
    const btnPrev = document.getElementById('btnPrevPage');
    const btnNext = document.getElementById('btnNextPage');
    if (btnPrev) {
      btnPrev.addEventListener('click', () => {
        if (this.currentPage > 1) {
          this.currentPage--;
          this.loadRecords();
        }
      });
    }
    if (btnNext) {
      btnNext.addEventListener('click', () => {
        this.currentPage++;
        this.loadRecords();
      });
    }

    // 3. Selecionar Todos os Checkboxes
    const chkAll = document.getElementById('chkSelectAll');
    if (chkAll) {
      chkAll.addEventListener('change', (e) => {
        const checkboxes = document.querySelectorAll('.row-select-chk');
        checkboxes.forEach(cb => {
          cb.checked = e.target.checked;
          const id = parseInt(cb.dataset.id);
          if (e.target.checked) {
            this.selectedIds.add(id);
          } else {
            this.selectedIds.delete(id);
          }
        });
        this.updateSelectedUI();
      });
    }

    // 4. Excluir Selecionados
    const btnDel = document.getElementById('btnDeleteSelected');
    if (btnDel) {
      btnDel.addEventListener('click', () => this.deleteSelected());
    }

    // 5. Purga Total do Banco
    const btnPurge = document.getElementById('btnPurgeAllDb');
    if (btnPurge) {
      btnPurge.addEventListener('click', () => this.purgeAll());
    }
  },

  async loadRecords() {
    const tbody = document.getElementById('tableRecordsBody');
    if (!tbody) return;

    tbody.innerHTML = `<tr><td colspan="16" style="text-align:center; padding:30px; color:var(--text-muted);">Consultando SQLite...</td></tr>`;

    try {
      const res = await fetch(`/api/records/list?page=${this.currentPage}&size=${this.pageSize}`);
      const data = await res.json();

      this.totalRecords = data.total;
      document.getElementById('lblTotalRecords').innerText = this.totalRecords;
      document.getElementById('lblCurrentPage').innerText = this.currentPage;

      // Estado dos botões de paginação
      const btnPrev = document.getElementById('btnPrevPage');
      const btnNext = document.getElementById('btnNextPage');
      if (btnPrev) btnPrev.disabled = (this.currentPage <= 1);
      if (btnNext) btnNext.disabled = (this.pageSize <= 0 || (this.currentPage * this.pageSize) >= this.totalRecords);

      if (!data.records || !data.records.length) {
        tbody.innerHTML = `<tr><td colspan="16" style="text-align:center; padding:30px; color:var(--text-muted);">Nenhum registo persistido no banco de dados.</td></tr>`;
        return;
      }

      tbody.innerHTML = '';
      data.records.forEach(r => {
        const isChecked = this.selectedIds.has(r.id);
        const tr = document.createElement('tr');
        tr.innerHTML = `
          <td style="text-align:center;">
            <input type="checkbox" class="row-select-chk" data-id="${r.id}" ${isChecked ? 'checked' : ''}>
          </td>
          <td style="font-family:monospace; color:var(--accent-cyan); font-weight:bold;">#${r.id}</td>
          <td style="font-family:monospace; font-size:0.75rem;">${r.timestamp}</td>
          <td style="font-family:monospace;">${r.session_id}</td>
          <td style="font-family:monospace;">${r.uptime_sec}s</td>
          <td style="font-weight:bold; color:var(--accent-cyan);">${Number(r.temperature).toFixed(1)}</td>
          <td style="color:var(--accent-blue);">${Number(r.humidity).toFixed(1)}</td>
          <td style="color:var(--accent-amber);">${r.heat_index ? Number(r.heat_index).toFixed(1) : '--'}</td>
          <td style="color:#60a5fa;">${r.dew_point ? Number(r.dew_point).toFixed(1) : '--'}</td>
          <td style="color:var(--accent-emerald); font-weight:bold;">${r.mq135_raw}</td>
          <td>${r.gas_ppm ? Number(r.gas_ppm).toFixed(0) : '--'}</td>
          <td>
            <span class="tag-status ${r.rain_detected ? 'tag-alert' : 'tag-ok'}">
              ${r.rain_detected ? 'CHUVA' : 'SECO'}
            </span>
          </td>
          <td>
            <span class="tag-status ${r.flame_detected ? 'tag-danger' : 'tag-ok'}">
              ${r.flame_detected ? 'FOGO' : 'SEGURO'}
            </span>
          </td>
          <td>
            <span class="tag-status ${r.is_anomaly ? 'tag-anomaly' : 'tag-ok'}">
              ${r.is_anomaly ? 'ANOMALIA' : 'NORMAL'}
            </span>
          </td>
          <td style="font-size:0.7rem; font-family:monospace; color:var(--text-muted);">${r.source}</td>
          <td style="text-align:center;">
            <button class="btn-action-sm" onclick="RecordsManager.deleteSingle(${r.id})" title="Excluir este registo">🗑️</button>
          </td>
        `;

        const chk = tr.querySelector('.row-select-chk');
        chk.addEventListener('change', (e) => {
          const id = parseInt(e.target.dataset.id);
          if (e.target.checked) {
            this.selectedIds.add(id);
          } else {
            this.selectedIds.delete(id);
          }
          this.updateSelectedUI();
        });

        tbody.appendChild(tr);
      });

    } catch (e) {
      tbody.innerHTML = `<tr><td colspan="16" style="text-align:center; color:var(--accent-rose); padding:20px;">Falha ao carregar registos.</td></tr>`;
    }
  },

  updateSelectedUI() {
    const btnDel = document.getElementById('btnDeleteSelected');
    const txtCount = document.getElementById('txtSelectedCount');
    const chkAll = document.getElementById('chkSelectAll');

    if (txtCount) txtCount.innerText = this.selectedIds.size;
    if (btnDel) btnDel.disabled = (this.selectedIds.size === 0);

    const checkboxes = document.querySelectorAll('.row-select-chk');
    if (chkAll && checkboxes.length > 0) {
      const allChecked = Array.from(checkboxes).every(cb => cb.checked);
      chkAll.checked = allChecked;
    }
  },

  async deleteSingle(id) {
    if (!confirm(`Confirma a exclusão do registo #${id}?`)) return;
    try {
      const res = await fetch('/api/records/delete', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ids: [id] })
      });
      const data = await res.json();
      if (data.status === 'success') {
        this.selectedIds.delete(id);
        this.updateSelectedUI();
        this.loadRecords();
      }
    } catch (err) {
      alert('Erro ao excluir registo.');
    }
  },

  async deleteSelected() {
    const ids = Array.from(this.selectedIds);
    if (!ids.length) return;
    if (!confirm(`Confirma a exclusão em massa de ${ids.length} registos?`)) return;

    try {
      const res = await fetch('/api/records/delete', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ids })
      });
      const data = await res.json();
      if (data.status === 'success') {
        alert(`${data.deleted_count} registos eliminados com sucesso.`);
        this.selectedIds.clear();
        this.updateSelectedUI();
        this.loadRecords();
      }
    } catch (err) {
      alert('Falha na exclusão dos registos.');
    }
  },

  async purgeAll() {
    if (!confirm('PERIGO: Esta ação irá APAGAR TODOS OS REGISTOS DO BANCO DE DADOS LOCAL! Confirmar?')) return;
    try {
      const res = await fetch('/api/records/purge_all', { method: 'POST' });
      const data = await res.json();
      if (data.status === 'success') {
        alert(`Banco limpo: ${data.purged} registos apagados.`);
        this.selectedIds.clear();
        this.currentPage = 1;
        this.updateSelectedUI();
        this.loadRecords();
      }
    } catch (err) {
      alert('Erro ao purgar base de dados.');
    }
  }
};

document.addEventListener('DOMContentLoaded', () => {
  RecordsManager.init();
});