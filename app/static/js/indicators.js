/**
 * Gestão dos Cartões de KPI, Medidor SVG e Síntese de Áudio Silenciável
 */
const Indicators = {
  soundEnabled: true,
  audioCtx: null,

  initAudio() {
    if (!this.audioCtx && typeof window.AudioContext !== 'undefined') {
      const AudioContextClass = window.AudioContext || window.webkitAudioContext;
      this.audioCtx = new AudioContextClass();
    }
    if (this.audioCtx && this.audioCtx.state === 'suspended') {
      this.audioCtx.resume();
    }
  },

  playAlarmBeep(freq = 880, duration = 120) {
    if (!this.soundEnabled) return;
    try {
      this.initAudio();
      if (!this.audioCtx) return;
      const osc = this.audioCtx.createOscillator();
      const gain = this.audioCtx.createGain();
      osc.type = 'sawtooth';
      osc.frequency.setValueAtTime(freq, this.audioCtx.currentTime);
      gain.gain.setValueAtTime(0.06, this.audioCtx.currentTime);
      gain.gain.exponentialRampToValueAtTime(0.0001, this.audioCtx.currentTime + (duration / 1000));
      osc.connect(gain);
      gain.connect(this.audioCtx.destination);
      osc.start();
      osc.stop(this.audioCtx.currentTime + (duration / 1000));
    } catch (e) {
      // Navegador bloqueou áudio antes da primeira interação do utilizador
    }
  },

  toggleSound() {
    this.soundEnabled = !this.soundEnabled;
    const btn = document.getElementById('btnToggleSound');
    const txt = document.getElementById('soundText');
    const icon = document.getElementById('soundIcon');
    if (!btn || !txt || !icon) return this.soundEnabled;

    if (this.soundEnabled) {
      txt.innerText = 'SOM ATIVO';
      icon.innerText = '🔊';
      btn.style.color = 'var(--text-main)';
      btn.style.borderColor = 'rgba(56, 189, 248, 0.3)';
    } else {
      txt.innerText = 'MUTADO';
      icon.innerText = '🔇';
      btn.style.color = 'var(--text-muted)';
      btn.style.borderColor = 'rgba(71, 85, 105, 0.4)';
    }
    return this.soundEnabled;
  },

  updateCards(d) {
    if (!d) return;

    // 1. Valores Básicos
    const elTemp = document.getElementById('kpiTemp');
    const elHum = document.getElementById('kpiHum');
    const elGas = document.getElementById('kpiGasRaw');

    if (elTemp && d.temperature !== undefined) elTemp.innerText = `${Number(d.temperature).toFixed(1)} °C`;
    if (elHum && d.humidity !== undefined) elHum.innerText = `${Number(d.humidity).toFixed(1)} %`;
    if (elGas && d.mq135_raw !== undefined) elGas.innerText = d.mq135_raw;

    // 2. Indicadores Derivados
    const elHI = document.getElementById('kpiHeatIndex');
    const elDP = document.getElementById('kpiDewPoint');
    const elAbs = document.getElementById('kpiAbsHum');
    const elVap = document.getElementById('kpiVaporPress');
    const elPpm = document.getElementById('kpiGasPpm');

    if (elHI) {
      const val = d.heat_index !== undefined ? d.heat_index : d.temperature;
      elHI.innerText = `${Number(val).toFixed(1)} °C`;
    }
    if (elDP && d.dew_point !== undefined) elDP.innerText = `Orvalho: ${Number(d.dew_point).toFixed(1)} °C`;
    if (elAbs && d.absolute_humidity_g_m3 !== undefined) elAbs.innerText = `Abs: ${Number(d.absolute_humidity_g_m3).toFixed(1)} g/m³`;
    if (elVap && d.vapor_pressure_hpa !== undefined) elVap.innerText = `Pressão: ${Number(d.vapor_pressure_hpa).toFixed(1)} hPa`;
    
    if (elPpm) {
      const ppmVal = d.estimated_ppm || d.gas_ppm || 400;
      elPpm.innerText = `PPM: ~${Number(ppmVal).toFixed(0)} ppm`;
    }

    // 3. Chuva
    const elRain = document.getElementById('kpiRain');
    if (elRain) {
      const isRain = Boolean(d.rain_detected);
      elRain.innerText = isRain ? 'DETECTADA' : 'SECO';
      elRain.style.color = isRain ? 'var(--accent-cyan)' : 'var(--text-muted)';
      elRain.style.textShadow = isRain ? '0 0 15px rgba(6, 182, 212, 0.8)' : 'none';
    }

    // 4. Incêndio / Risco Crítico
    const elFlame = document.getElementById('kpiFlame');
    if (elFlame) {
      const isFlame = Boolean(d.flame_detected);
      if (isFlame) {
        elFlame.innerText = 'EMERGÊNCIA!';
        elFlame.style.color = 'var(--accent-rose)';
        elFlame.style.textShadow = '0 0 20px rgba(244, 63, 94, 0.9)';
        this.playAlarmBeep(1400, 300);
      } else {
        elFlame.innerText = 'SEGURO';
        elFlame.style.color = 'var(--accent-emerald)';
        elFlame.style.textShadow = 'none';
      }
    }

    // 5. Gauge Integrado
    this.updateGauge(d);
  },

  updateGauge(d) {
    let score = 0;
    if (d.flame_detected) score += 65;
    if (d.mq135_raw > 2000) score += Math.min(25, ((d.mq135_raw - 2000) / 2095) * 25);
    if (d.temperature > 40) score += 10;
    score = Math.min(100, Math.round(score));

    const circle = document.getElementById('gaugeRiskCircle');
    const valText = document.getElementById('gaugeRiskVal');
    const lblText = document.getElementById('gaugeRiskLabel');

    if (!circle || !valText || !lblText) return;

    const totalCircumference = 251.2;
    const offset = totalCircumference - (score / 100) * totalCircumference;
    circle.style.strokeDashoffset = offset;
    valText.innerText = score;

    if (score >= 60) {
      lblText.innerText = 'PERIGO CRÍTICO';
      lblText.style.color = 'var(--accent-rose)';
    } else if (score >= 30) {
      lblText.innerText = 'ALERTA';
      lblText.style.color = 'var(--accent-amber)';
    } else {
      lblText.innerText = 'NORMAL';
      lblText.style.color = 'var(--accent-emerald)';
    }
  }
};