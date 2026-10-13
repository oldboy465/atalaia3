/**
 * Motor Chart.js Seguro com Interpolação de Tendência e Atualização Contínua
 */
const ChartsEngine = {
  chartBio: null,
  chartGas: null,
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
              backgroundColor: 'rgba(6, 182, 212, 0.1)',
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
        options: commonOptions
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
              ...commonOptions.scales.y,
              position: 'left',
              title: { display: true, text: 'ADC 12-bit', color: '#64748b' }
            },
            y1: {
              position: 'right',
              grid: { drawOnChartArea: false },
              ticks: { color: '#f43f5e', font: { family: 'monospace', size: 9 } },
              title: { display: true, text: 'PPM', color: '#f43f5e' }
            }
          }
        }
      });
    }

    // 3. Detecção de Anomalias & Bollinger
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
        options: commonOptions
      });
    }
  },

  calculateLinearTrend(values) {
    const n = values.length;
    if (n < 2) return values.slice();
    let sumX = 0, sumY = 0, sumXY = 0, sumXX = 0;
    for (let i = 0; i < n; i++) {
      sumX += i;
      sumY += values[i];
      sumXY += i * values[i];
      sumXX += i * i;
    }
    const denom = n * sumXX - sumX * sumX;
    const slope = denom !== 0 ? (n * sumXY - sumX * sumY) / denom : 0;
    const intercept = (sumY - slope * sumX) / n;
    return values.map((_, i) => Number((slope * i + intercept).toFixed(2)));
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

  populateHistory(records) {
    if (!records || !records.length) return;
    const labels = records.map(r => (r.timestamp ? r.timestamp.split(' ')[1] : `${r.uptime_sec}s`));
    const temps = records.map(r => r.temperature);
    const mqs = records.map(r => r.mq135_raw);

    // Bioclimático
    if (this.chartBio) {
      this.chartBio.data.labels = labels;
      this.chartBio.data.datasets[0].data = temps;
      this.chartBio.data.datasets[1].data = this.calculateLinearTrend(temps);
      this.chartBio.data.datasets[2].data = records.map(r => r.heat_index || r.temperature);
      this.chartBio.data.datasets[3].data = records.map(r => r.dew_point || 0);
      this.chartBio.update('none');
    }

    // Gases
    if (this.chartGas) {
      this.chartGas.data.labels = labels;
      this.chartGas.data.datasets[0].data = mqs;
      this.chartGas.data.datasets[1].data = this.calculateLinearTrend(mqs);
      this.chartGas.data.datasets[2].data = records.map(r => r.gas_ppm || r.estimated_ppm || 400);
      this.chartGas.update('none');
    }

    // Anomalias
    if (this.chartAnom) {
      this.chartAnom.data.labels = labels;
      this.chartAnom.data.datasets[0].data = temps;
      this.chartAnom.data.datasets[0].anomalyFlags = records.map(r => Boolean(r.is_anomaly));
      this.chartAnom.data.datasets[1].data = records.map(r => Number((r.temperature + 2.5).toFixed(2)));
      this.chartAnom.data.datasets[2].data = records.map(r => Number((r.temperature - 2.5).toFixed(2)));
      this.chartAnom.update('none');
    }
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