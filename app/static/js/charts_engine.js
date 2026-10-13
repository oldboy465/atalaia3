/**
 * Motor de Gráficos Chart.js com Animações Fluidas e Renderização Contínua
 */
const ChartsEngine = {
  chartBio: null,
  chartGas: null,
  chartAnom: null,

  initCharts() {
    const commonOptions = {
      responsive: true,
      maintainAspectRatio: false,
      animation: {
        duration: 800,
        easing: 'easeOutQuart'
      },
      interaction: {
        mode: 'index',
        intersect: false
      },
      plugins: {
        legend: {
          labels: { color: '#94a3b8', font: { family: 'monospace', size: 11 } }
        },
        tooltip: {
          backgroundColor: 'rgba(15, 23, 42, 0.9)',
          borderColor: '#38bdf8',
          borderWidth: 1,
          titleFont: { family: 'monospace' },
          bodyFont: { family: 'monospace' }
        }
      },
      scales: {
        x: {
          grid: { color: 'rgba(51, 65, 85, 0.25)' },
          ticks: { color: '#64748b', font: { family: 'monospace', size: 10 }, maxRotation: 0 }
        },
        y: {
          grid: { color: 'rgba(51, 65, 85, 0.25)' },
          ticks: { color: '#64748b', font: { family: 'monospace', size: 10 } }
        }
      }
    };

    // 1. Gráfico Bioclimático (Temperatura, Heat Index e Dew Point)
    const ctxBio = document.getElementById('chartBioclimatic').getContext('2d');
    this.chartBio = new Chart(ctxBio, {
      type: 'line',
      data: {
        labels: [],
        datasets: [
          {
            label: 'Temp (°C)',
            data: [],
            borderColor: '#22d3ee',
            backgroundColor: 'rgba(34, 211, 238, 0.1)',
            borderWidth: 2,
            tension: 0.35,
            fill: true
          },
          {
            label: 'Heat Index (°C)',
            data: [],
            borderColor: '#fbbf24',
            borderWidth: 2,
            borderDash: [4, 4],
            tension: 0.35
          },
          {
            label: 'Dew Point (°C)',
            data: [],
            borderColor: '#60a5fa',
            borderWidth: 1.5,
            tension: 0.35
          }
        ]
      },
      options: commonOptions
    });

    // 2. Gráfico de Gases MQ-135 e PPM
    const ctxGas = document.getElementById('chartGases').getContext('2d');
    this.chartGas = new Chart(ctxGas, {
      type: 'line',
      data: {
        labels: [],
        datasets: [
          {
            label: 'MQ-135 (ADC Raw)',
            data: [],
            borderColor: '#34d399',
            backgroundColor: 'rgba(52, 211, 153, 0.08)',
            borderWidth: 2,
            yAxisID: 'y',
            tension: 0.3,
            fill: true
          },
          {
            label: 'Estimativa PPM',
            data: [],
            borderColor: '#f43f5e',
            borderWidth: 1.5,
            yAxisID: 'y1',
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
            ticks: { color: '#f43f5e', font: { family: 'monospace', size: 10 } },
            title: { display: true, text: 'PPM', color: '#f43f5e' }
          }
        }
      }
    });

    // 3. Gráfico de Bollinger Bands & Detecção de Anomalias
    const ctxAnom = document.getElementById('chartAnomalies').getContext('2d');
    this.chartAnom = new Chart(ctxAnom, {
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
              const idx = ctx.dataIndex;
              const meta = ctx.chart.data.datasets[0].anomalyFlags;
              return (meta && meta[idx]) ? 6 : 0;
            },
            pointBackgroundColor: '#ef4444',
            pointBorderColor: '#ffffff',
            pointBorderWidth: 2,
            tension: 0.25
          },
          {
            label: 'Bollinger Superior',
            data: [],
            borderColor: 'rgba(168, 85, 247, 0.3)',
            borderDash: [3, 3],
            borderWidth: 1,
            pointRadius: 0
          },
          {
            label: 'Bollinger Inferior',
            data: [],
            borderColor: 'rgba(168, 85, 247, 0.3)',
            borderDash: [3, 3],
            borderWidth: 1,
            pointRadius: 0
          }
        ]
      },
      options: commonOptions
    });
  },

  populateHistory(records) {
    if (!records || !records.length) return;

    const labels = records.map(r => (r.timestamp ? r.timestamp.split(' ')[1] : `${r.uptime_sec}s`));
    
    // Bio
    this.chartBio.data.labels = labels;
    this.chartBio.data.datasets[0].data = records.map(r => r.temperature);
    this.chartBio.data.datasets[1].data = records.map(r => r.heat_index || r.temperature);
    this.chartBio.data.datasets[2].data = records.map(r => r.dew_point || 0);
    this.chartBio.update('none');

    // Gas
    this.chartGas.data.labels = labels;
    this.chartGas.data.datasets[0].data = records.map(r => r.mq135_raw);
    this.chartGas.data.datasets[1].data = records.map(r => r.estimated_ppm || r.gas_ppm || 400);
    this.chartGas.update('none');

    // Anomalies
    this.chartAnom.data.labels = labels;
    this.chartAnom.data.datasets[0].data = records.map(r => r.temperature);
    this.chartAnom.data.datasets[0].anomalyFlags = records.map(r => Boolean(r.is_anomaly));
    this.chartAnom.data.datasets[1].data = records.map(r => (r.temperature + 2.5).toFixed(2));
    this.chartAnom.data.datasets[2].data = records.map(r => (r.temperature - 2.5).toFixed(2));
    this.chartAnom.update('none');
  },

  pushLiveData(sample) {
    const label = sample.timestamp ? sample.timestamp.split(' ')[1] : `${sample.uptime_sec}s`;
    const maxPoints = 50;

    const pushShift = (chart, dsIndex, val) => {
      const ds = chart.data.datasets[dsIndex];
      ds.data.push(val);
      if (ds.data.length > maxPoints) ds.data.shift();
    };

    // Atualiza Labels
    [this.chartBio, this.chartGas, this.chartAnom].forEach(c => {
      c.data.labels.push(label);
      if (c.data.labels.length > maxPoints) c.data.labels.shift();
    });

    // Bio
    pushShift(this.chartBio, 0, sample.temperature);
    pushShift(this.chartBio, 1, sample.heat_index || sample.temperature);
    pushShift(this.chartBio, 2, sample.dew_point || 0);
    this.chartBio.update('none');

    // Gas
    pushShift(this.chartGas, 0, sample.mq135_raw);
    pushShift(this.chartGas, 1, sample.estimated_ppm || 400);
    this.chartGas.update('none');

    // Anomaly
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
};