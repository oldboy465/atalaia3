/**
 * Motor Chart.js Seguro com Interpolação de Tendência, Scatter Físico, Barras Recentes e Estatística Descritiva
 */
const ChartsEngine = {
  chartBio: null,
  chartGas: null,
  chartScatter: null,
  chartBar: null,
  chartAnom: null,
  showTrendLines: true,

  initCharts() {
    if (typeof Chart === 'undefined') {
      console.error('[CHARTS] Biblioteca Chart.js não encontrada.');
      return;
    }

    const commonOptions = {
      responsive: true,
      maintainAspectRatio: false,
      animation: { duration: 300 },
      interaction: { mode: 'index', intersect: false },
      plugins: {
        legend: {
          labels: { color: '#94a3b8', font: { family: 'monospace', size: 10 } }
        },
        tooltip: {
          backgroundColor: 'rgba(10, 18, 34, 0.95)',
          borderColor: '#06b6d4',
          borderWidth: 1
        }
      },
      scales: {
        x: {
          grid: { color: 'rgba(51, 65, 85, 0.25)' },
          ticks: { color: '#64748b', font: { family: 'monospace', size: 9 }, maxRotation: 0 }
        },
        y: {
          grid: { color: 'rgba(51, 65, 85, 0.25)' },
          ticks: { color: '#64748b', font: { family: 'monospace', size: 9 } }
        }
      }
    };

    // 1. Bioclimático
    const elBio = document.getElementById('chartBioclimatic');
    if (elBio) {
      this.chartBio = new Chart(elBio.getContext('2d'), {
        type: 'line',
        data: {
          labels: [],
          datasets: [
            {
              label: 'Temp (°C)',
              data: [],
              borderColor: '#06b6d4',
              backgroundColor: 'rgba(6, 182, 212, 0.12)',
              borderWidth: 2,
              fill: true,
              tension: 0.3
            },
            {
              label: 'Tendência Linear',
              data: [],
              borderColor: '#f43f5e',
              borderWidth: 1.5,
              borderDash: [5, 4],
              pointRadius: 0,
              hidden: !this.showTrendLines
            },
            {
              label: 'Sensação Térmica (HI)',
              data: [],
              borderColor: '#f59e0b',
              borderWidth: 1.5,
              borderDash: [3, 3],
              pointRadius: 0,
              tension: 0.3
            },
            {
              label: 'Ponto Orvalho (°C)',
              data: [],
              borderColor: '#38bdf8',
              borderWidth: 1.5,
              pointRadius: 0,
              tension: 0.3
            }
          ]
        },
        options: {
          ...commonOptions,
          scales: {
            ...commonOptions.scales,
            y: {
              ...commonOptions.scales.y,
              suggestedMin: 15,
              suggestedMax: 45
            }
          }
        }
      });
    }

    // 2. Gases MQ-135 e PPM
    const elGas = document.getElementById('chartGases');
    if (elGas) {
      this.chartGas = new Chart(elGas.getContext('2d'), {
        type: 'line',
        data: {
          labels: [],
          datasets: [
            {
              label: 'MQ-135 (ADC Raw)',
              data: [],
              borderColor: '#10b981',
              backgroundColor: 'rgba(16, 185, 129, 0.08)',
              borderWidth: 2,
              fill: true,
              yAxisID: 'y',
              tension: 0.3
            },
            {
              label: 'Tendência MQ',
              data: [],
              borderColor: '#a855f7',
              borderWidth: 1.5,
              borderDash: [5, 4],
              pointRadius: 0,
              yAxisID: 'y',
              hidden: !this.showTrendLines
            },
            {
              label: 'PPM Estimado',
              data: [],
              borderColor: '#f43f5e',
              borderWidth: 1.5,
              yAxisID: 'y1',
              pointRadius: 0,
              tension: 0.3
            }
          ]
        },
        options: {
          ...commonOptions,
          scales: {
            ...commonOptions.scales,
            y: {
              position: 'left',
              suggestedMin: 200,
              suggestedMax: 2500,
              grid: { color: 'rgba(51, 65, 85, 0.25)' },
              ticks: { color: '#10b981', font: { family: 'monospace', size: 9 } },
              title: { display: true, text: 'ADC 12-bit', color: '#10b981' }
            },
            y1: {
              position: 'right',
              suggestedMin: 200,
              suggestedMax: 1500,
              grid: { drawOnChartArea: false },
              ticks: { color: '#f43f5e', font: { family: 'monospace', size: 9 } },
              title: { display: true, text: 'PPM', color: '#f43f5e' }
            }
          }
        }
      });
    }

    // 3. Dispersão: Umidade vs PPM Estimado (Resgatado do Atalaia 1)
    const elScatter = document.getElementById('chartScatterGasHum');
    if (elScatter) {
      this.chartScatter = new Chart(elScatter.getContext('2d'), {
        type: 'scatter',
        data: {
          datasets: [{
            label: 'Amostras (Umid vs PPM)',
            data: [],
            backgroundColor: 'rgba(245, 158, 11, 0.65)',
            borderColor: '#f59e0b',
            borderWidth: 1,
            pointRadius: 5,
            pointHoverRadius: 8
          }]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          animation: { duration: 250 },
          plugins: {
            legend: { labels: { color: '#94a3b8', font: { family: 'monospace', size: 10 } } },
            tooltip: {
              callbacks: {
                label: (ctx) => `Umid: ${ctx.parsed.x}% | Gás: ${ctx.parsed.y} ppm`
              }
            }
          },
          scales: {
            x: {
              title: { display: true, text: 'Umidade Relativa (%)', color: '#64748b', font: { family: 'monospace', size: 10 } },
              grid: { color: 'rgba(51, 65, 85, 0.25)' },
              ticks: { color: '#94a3b8', font: { family: 'monospace', size: 9 } },
              suggestedMin: 30,
              suggestedMax: 100
            },
            y: {
              title: { display: true, text: 'CO₂ Equivalente (PPM)', color: '#64748b', font: { family: 'monospace', size: 10 } },
              grid: { color: 'rgba(51, 65, 85, 0.25)' },
              ticks: { color: '#94a3b8', font: { family: 'monospace', size: 9 } },
              suggestedMin: 300,
              suggestedMax: 2000
            }
          }
        }
      });
    }

    // 4. Barras de Carga Recente: MQ-135 vs PPM (Resgatado do Atalaia 1)
    const elBar = document.getElementById('chartBarGasRecent');
    if (elBar) {
      this.chartBar = new Chart(elBar.getContext('2d'), {
        type: 'bar',
        data: {
          labels: [],
          datasets: [
            {
              label: 'MQ-135 (RAW)',
              data: [],
              backgroundColor: 'rgba(56, 189, 248, 0.7)',
              borderColor: '#38bdf8',
              borderWidth: 1
            },
            {
              label: 'CO₂ PPM',
              data: [],
              backgroundColor: 'rgba(234, 179, 8, 0.7)',
              borderColor: '#eab308',
              borderWidth: 1
            }
          ]
        },
        options: {
          responsive: true,
          maintainAspectRatio: false,
          animation: { duration: 300 },
          plugins: {
            legend: { labels: { color: '#94a3b8', font: { family: 'monospace', size: 10 } } }
          },
          scales: {
            x: {
              grid: { color: 'rgba(51, 65, 85, 0.25)' },
              ticks: { color: '#64748b', font: { family: 'monospace', size: 8 } }
            },
            y: {
              grid: { color: 'rgba(51, 65, 85, 0.25)' },
              ticks: { color: '#64748b', font: { family: 'monospace', size: 9 } }
            }
          }
        }
      });
    }

    // 5. Anomalias & Bollinger Bands
    const elAnom = document.getElementById('chartAnomalies');
    if (elAnom) {
      this.chartAnom = new Chart(elAnom.getContext('2d'), {
        type: 'line',
        data: {
          labels: [],
          datasets: [
            {
              label: 'Temperatura Real',
              data: [],
              borderColor: '#a855f7',
              borderWidth: 2,
              pointRadius: (ctx) => {
                const meta = ctx.chart.data.datasets[0].anomalyFlags;
                return (meta && meta[ctx.dataIndex]) ? 6 : 0;
              },
              pointBackgroundColor: '#f43f5e',
              pointBorderColor: '#ffffff',
              pointBorderWidth: 2,
              tension: 0.25
            },
            {
              label: 'Bollinger Superior',
              data: [],
              borderColor: 'rgba(168, 85, 247, 0.35)',
              borderDash: [3, 3],
              borderWidth: 1,
              pointRadius: 0
            },
            {
              label: 'Bollinger Inferior',
              data: [],
              borderColor: 'rgba(168, 85, 247, 0.35)',
              borderDash: [3, 3],
              borderWidth: 1,
              pointRadius: 0
            }
          ]
        },
        options: {
          ...commonOptions,
          scales: {
            ...commonOptions.scales,
            y: {
              ...commonOptions.scales.y,
              suggestedMin: 15,
              suggestedMax: 45
            }
          }
        }
      });
    }
  },

  calculateLinearTrend(values) {
    const validPoints = values.filter(v => v !== null && !isNaN(v) && isFinite(v));
    const n = validPoints.length;
    if (n < 2) return values.slice();

    let sumX = 0, sumY = 0, sumXY = 0, sumXX = 0;
    for (let i = 0; i < n; i++) {
      sumX += i;
      sumY += validPoints[i];
      sumXY += i * validPoints[i];
      sumXX += i * i;
    }
    const denom = (n * sumXX) - (sumX * sumX);
    if (Math.abs(denom) < 0.000001) return values.slice();

    const slope = (n * sumXY - sumX * sumY) / denom;
    const intercept = (sumY - slope * sumX) / n;

    return values.map((v, i) => {
      if (v === null || isNaN(v)) return null;
      const res = slope * i + intercept;
      return isFinite(res) ? Number(res.toFixed(2)) : v;
    });
  },

  toggleTrendVisibility() {
    this.showTrendLines = !this.showTrendLines;
    if (this.chartBio) {
      this.chartBio.data.datasets[1].hidden = !this.showTrendLines;
      this.chartBio.update('none');
    }
    if (this.chartGas) {
      this.chartGas.data.datasets[1].hidden = !this.showTrendLines;
      this.chartGas.update('none');
    }
    return this.showTrendLines;
  },

  updateStatsTable(records) {
    const tbody = document.getElementById('statsTableBody');
    if (!tbody || !records || !records.length) return;

    const computeMetrics = (vals) => {
      const arr = vals.filter(v => v !== null && !isNaN(v)).map(Number);
      const n = arr.length;
      if (n === 0) return null;
      
      const mean = arr.reduce((a, b) => a + b, 0) / n;
      const sorted = [...arr].sort((a, b) => a - b);
      const median = n % 2 === 0 ? (sorted[n / 2 - 1] + sorted[n / 2]) / 2 : sorted[Math.floor(n / 2)];
      const min = sorted[0];
      const max = sorted[n - 1];
      
      const variance = n > 1 ? arr.reduce((acc, v) => acc + Math.pow(v - mean, 2), 0) / (n - 1) : 0;
      const sd = Math.sqrt(variance);
      const cv = mean !== 0 ? (sd / mean) * 100 : 0;
      
      const se = n > 1 ? sd / Math.sqrt(n) : 0;
      const tCrit = 1.96; // Aproximação 95%
      const icLow = (mean - tCrit * se).toFixed(2);
      const icHigh = (mean + tCrit * se).toFixed(2);

      return {
        n,
        mean: mean.toFixed(2),
        median: median.toFixed(2),
        min: min.toFixed(2),
        max: max.toFixed(2),
        sd: sd.toFixed(2),
        cv: cv.toFixed(2),
        ic: `[${icLow}, ${icHigh}]`
      };
    };

    const vars = [
      { name: 'Temperatura (°C)', vals: records.map(r => r.temperature) },
      { name: 'Umidade Relativa (%)', vals: records.map(r => r.humidity) },
      { name: 'Sensação Térmica (°C)', vals: records.map(r => r.heat_index || r.temperature) },
      { name: 'Ponto de Orvalho (°C)', vals: records.map(r => r.dew_point) },
      { name: 'Gás MQ-135 (RAW)', vals: records.map(r => r.mq135_raw) },
      { name: 'CO₂ Estimado (PPM)', vals: records.map(r => r.estimated_ppm || r.gas_ppm || 400) }
    ];

    tbody.innerHTML = vars.map(v => {
      const s = computeMetrics(v.vals);
      if (!s) return '';
      return `
        <tr>
          <td style="font-weight: bold; color: #f8fafc;">${v.name}</td>
          <td style="color: var(--text-muted); font-family: monospace;">${s.n}</td>
          <td style="color: var(--accent-cyan); font-weight: bold; font-family: monospace;">${s.mean}</td>
          <td style="font-family: monospace;">${s.median}</td>
          <td style="color: var(--text-dim); font-family: monospace;">${s.min}</td>
          <td style="color: var(--text-dim); font-family: monospace;">${s.max}</td>
          <td style="color: var(--text-muted); font-family: monospace;">${s.sd}</td>
          <td style="color: var(--accent-emerald); font-weight: bold; font-family: monospace;">${s.cv}%</td>
          <td style="text-align: center; color: var(--accent-blue); font-family: monospace; font-size: 0.72rem;">${s.ic}</td>
        </tr>
      `;
    }).join('');
  },

  populateHistory(records) {
    if (!records || !records.length) return;
    const labels = records.map(r => (r.timestamp ? r.timestamp.split(' ')[1] : `${r.uptime_sec}s`));
    const temps = records.map(r => r.temperature);
    const hums = records.map(r => r.humidity);
    const mqs = records.map(r => r.mq135_raw);
    const ppms = records.map(r => r.estimated_ppm || r.gas_ppm || 400);

    // 1. Bioclimático
    if (this.chartBio) {
      this.chartBio.data.labels = labels;
      this.chartBio.data.datasets[0].data = temps;
      this.chartBio.data.datasets[1].data = this.calculateLinearTrend(temps);
      this.chartBio.data.datasets[2].data = records.map(r => r.heat_index || r.temperature);
      this.chartBio.data.datasets[3].data = records.map(r => r.dew_point || 0);
      this.chartBio.update('none');
    }

    // 2. Gases
    if (this.chartGas) {
      this.chartGas.data.labels = labels;
      this.chartGas.data.datasets[0].data = mqs;
      this.chartGas.data.datasets[1].data = this.calculateLinearTrend(mqs);
      this.chartGas.data.datasets[2].data = ppms;
      this.chartGas.update('none');
    }

    // 3. Scatter (Dispersão: Umidade vs PPM)
    if (this.chartScatter) {
      this.chartScatter.data.datasets[0].data = records.map(r => ({
        x: r.humidity,
        y: r.estimated_ppm || r.gas_ppm || 400
      }));
      this.chartScatter.update('none');
    }

    // 4. Barras Recentes (Últimos 12 pontos)
    if (this.chartBar) {
      const sliceRec = records.slice(-12);
      this.chartBar.data.labels = sliceRec.map(r => (r.timestamp ? r.timestamp.split(' ')[1] : `${r.uptime_sec}s`));
      this.chartBar.data.datasets[0].data = sliceRec.map(r => r.mq135_raw);
      this.chartBar.data.datasets[1].data = sliceRec.map(r => r.estimated_ppm || r.gas_ppm || 400);
      this.chartBar.update('none');
    }

    // 5. Anomalias
    if (this.chartAnom) {
      this.chartAnom.data.labels = labels;
      this.chartAnom.data.datasets[0].data = temps;
      this.chartAnom.data.datasets[0].anomalyFlags = records.map(r => Boolean(r.is_anomaly));
      this.chartAnom.data.datasets[1].data = records.map(r => Number((r.temperature + 2.5).toFixed(2)));
      this.chartAnom.data.datasets[2].data = records.map(r => Number((r.temperature - 2.5).toFixed(2)));
      this.chartAnom.update('none');
    }

    // 6. Atualiza Tabela Estatística Resgatada
    this.updateStatsTable(records);
  },

  pushLiveData(sample) {
    const label = sample.timestamp ? sample.timestamp.split(' ')[1] : `${sample.uptime_sec}s`;
    const maxPoints = 40;

    const pushShift = (chart, dsIdx, val) => {
      if (!chart) return;
      const ds = chart.data.datasets[dsIdx];
      ds.data.push(val);
      if (ds.data.length > maxPoints) ds.data.shift();
    };

    [this.chartBio, this.chartGas, this.chartAnom].forEach(c => {
      if (!c) return;
      c.data.labels.push(label);
      if (c.data.labels.length > maxPoints) c.data.labels.shift();
    });

    if (this.chartBio) {
      pushShift(this.chartBio, 0, sample.temperature);
      this.chartBio.data.datasets[1].data = this.calculateLinearTrend(this.chartBio.data.datasets[0].data);
      pushShift(this.chartBio, 2, sample.heat_index || sample.temperature);
      pushShift(this.chartBio, 3, sample.dew_point || 0);
      this.chartBio.update('none');
    }

    if (this.chartGas) {
      pushShift(this.chartGas, 0, sample.mq135_raw);
      this.chartGas.data.datasets[1].data = this.calculateLinearTrend(this.chartGas.data.datasets[0].data);
      pushShift(this.chartGas, 2, sample.estimated_ppm || 400);
      this.chartGas.update('none');
    }

    // Scatter dinâmico: adiciona o ponto recente
    if (this.chartScatter) {
      const scData = this.chartScatter.data.datasets[0].data;
      scData.push({ x: sample.humidity, y: sample.estimated_ppm || 400 });
      if (scData.length > 50) scData.shift();
      this.chartScatter.update('none');
    }

    // Barras recentes dinâmicas
    if (this.chartBar) {
      const bLabels = this.chartBar.data.labels;
      bLabels.push(label);
      if (bLabels.length > 12) bLabels.shift();

      const bRaw = this.chartBar.data.datasets[0].data;
      bRaw.push(sample.mq135_raw);
      if (bRaw.length > 12) bRaw.shift();

      const bPpm = this.chartBar.data.datasets[1].data;
      bPpm.push(sample.estimated_ppm || 400);
      if (bPpm.length > 12) bPpm.shift();

      this.chartBar.update('none');
    }

    if (this.chartAnom) {
      const anomDs = this.chartAnom.data.datasets[0];
      anomDs.data.push(sample.temperature);
      if (!anomDs.anomalyFlags) anomDs.anomalyFlags = [];
      anomDs.anomalyFlags.push(Boolean(sample.is_anomaly));
      if (anomDs.data.length > maxPoints) {
        anomDs.data.shift();
        anomDs.anomalyFlags.shift();
      }
      pushShift(this.chartAnom, 1, Number((sample.temperature + 2.5).toFixed(2)));
      pushShift(this.chartAnom, 2, Number((sample.temperature - 2.5).toFixed(2)));
      this.chartAnom.update('none');
    }
  }
};