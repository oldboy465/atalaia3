/**
 * Gestão Expandida dos 12 Cartões de KPIs, Medidor SVG e Síntese Sonora
 * Inclui motor psicrométrico local para garantia de renderização sem valores nulos.
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
    } catch (e) {}
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

  // Motor termodinâmico de salvaguarda no front-end
  computeLocalPsychrometrics(t, h, mq) {
    // 1. Ponto de Orvalho (Magnus-Tetens)
    const a = 17.27, b = 237.7;
    const alpha = ((a * t) / (b + t)) + Math.log(Math.max(0.01, h) / 100.0);
    const dp = (b * alpha) / (a - alpha);

    // 2. Pressão de Vapor de Saturação e Real (hPa e kPa)
    const es = 6.112 * Math.exp((17.67 * t) / (t + 243.5));
    const ea = es * (h / 100.0);
    const vpd_kpa = Math.max(0, (es - ea) / 10.0);

    // 3. Umidade Absoluta (g/m³)
    const absHum = (ea * 216.7) / (t + 273.15);

    // 4. Margem de Condensação (°C)
    const condMargin = t - dp;

    // 5. Humidex
    const e_hmdx = 6.11 * Math.exp(5417.7530 * ((1.0 / 273.16) - (1.0 / (273.15 + dp))));
    const humidex = t + (5.0 / 9.0) * (e_hmdx - 10.0);

    // 6. Entalpia Específica (kJ/kg)
    const p_atm = 1013.25;
    const w = 0.622 * (ea / Math.max(1.0, (p_atm - ea)));
    const enthalpy = (1.006 * t) + (w * (2501.0 + (1.86 * t)));

    // 7. Tensão do Sensor MQ-135 e Rs
    const volt = (Math.max(1, Math.min(4095, mq)) / 4095.0) * 3.3;
    const rs = 10.0 * (3.3 - volt) / Math.max(0.001, volt);

    return {
      dew_point: dp,
      vpd_kpa: vpd_kpa,
      absolute_humidity_g_m3: absHum,
      condensation_margin_c: condMargin,
      humidex: humidex,
      enthalpy_kj_kg: enthalpy,
      vapor_pressure_hpa: ea,
      voltage_v: volt,
      sensor_rs_kohm: rs
    };
  },

  updateCards(d) {
    if (!d) return;

    const t = parseFloat(d.temperature) || 0.0;
    const h = parseFloat(d.humidity) || 0.0;
    const mq = parseInt(d.mq135_raw) || 0;

    // Calcula os derivados localmente caso não venham do backend
    const local = this.computeLocalPsychrometrics(t, h, mq);

    const dewPoint = d.dew_point !== undefined ? Number(d.dew_point) : local.dew_point;
    const heatIndex = d.heat_index !== undefined ? Number(d.heat_index) : t;
    const humidex = d.humidex !== undefined ? Number(d.humidex) : local.humidex;
    const absHum = d.absolute_humidity_g_m3 !== undefined ? Number(d.absolute_humidity_g_m3) : local.absolute_humidity_g_m3;
    const vpd = d.vpd_kpa !== undefined ? Number(d.vpd_kpa) : local.vpd_kpa;
    const vaporPress = d.vapor_pressure_hpa !== undefined ? Number(d.vapor_pressure_hpa) : local.vapor_pressure_hpa;
    const condMargin = d.condensation_margin_c !== undefined ? Number(d.condensation_margin_c) : local.condensation_margin_c;
    const enthalpy = d.enthalpy_kj_kg !== undefined ? Number(d.enthalpy_kj_kg) : local.enthalpy_kj_kg;
    const volt = d.voltage_v !== undefined ? Number(d.voltage_v) : local.voltage_v;
    const rs = d.sensor_rs_kohm !== undefined ? Number(d.sensor_rs_kohm) : local.sensor_rs_kohm;
    const ppm = d.estimated_ppm || d.gas_ppm || 400;

    // 1. Temperatura e Orvalho
    const elTemp = document.getElementById('kpiTemp');
    const elDP = document.getElementById('kpiDewPoint');
    if (elTemp) elTemp.innerText = `${t.toFixed(1)} °C`;
    if (elDP) elDP.innerText = `Ponto Orvalho: ${dewPoint.toFixed(1)} °C`;

    // 2. Umidade Relativa e Absoluta
    const elHum = document.getElementById('kpiHum');
    const elAbs = document.getElementById('kpiAbsHum');
    if (elHum) elHum.innerText = `${h.toFixed(1)} %`;
    if (elAbs) elAbs.innerText = `Absoluta: ${absHum.toFixed(1)} g/m³`;

    // 3. Sensação Térmica e Humidex
    const elHI = document.getElementById('kpiHeatIndex');
    const elHmdx = document.getElementById('kpiHumidex');
    if (elHI) elHI.innerText = `${heatIndex.toFixed(1)} °C`;
    if (elHmdx) elHmdx.innerText = `Humidex: ${humidex.toFixed(1)} °C`;

    // 4. Qualidade do Ar MQ-135 e PPM
    const elGas = document.getElementById('kpiGasRaw');
    const elPpm = document.getElementById('kpiGasPpm');
    if (elGas) elGas.innerText = mq;
    if (elPpm) elPpm.innerText = `PPM Estimado: ~${Number(ppm).toFixed(0)} ppm`;

    // 5. Chuva
    const elRain = document.getElementById('kpiRain');
    if (elRain) {
      const isRain = Boolean(d.rain_detected);
      elRain.innerText = isRain ? 'DETECTADA' : 'SECO';
      elRain.style.color = isRain ? 'var(--accent-cyan)' : 'var(--text-muted)';
      elRain.style.textShadow = isRain ? '0 0 15px rgba(6, 182, 212, 0.8)' : 'none';
    }

    // 6. Risco de Incêndio
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

    // 7. Margem de Condensação
    const elCondMargin = document.getElementById('kpiCondMargin');
    const elCondStatus = document.getElementById('kpiCondStatus');
    const elTxtCond = document.getElementById('txtCondMargin');
    if (elCondMargin) elCondMargin.innerText = `${condMargin.toFixed(1)} °C`;
    if (elTxtCond) elTxtCond.innerText = `${condMargin.toFixed(1)} °C`;
    if (elCondStatus) {
      if (condMargin < 2.0) {
        elCondStatus.innerText = 'Risco Condensação: IMINENTE';
        elCondStatus.style.color = 'var(--accent-rose)';
      } else if (condMargin < 5.0) {
        elCondStatus.innerText = 'Risco Condensação: MÉDIO';
        elCondStatus.style.color = 'var(--accent-amber)';
      } else {
        elCondStatus.innerText = 'Risco Condensação: BAIXO';
        elCondStatus.style.color = 'var(--accent-emerald)';
      }
    }

    // 8. Défice de Pressão de Vapor (VPD)
    const elVpd = document.getElementById('kpiVpd');
    const elTxtVpd = document.getElementById('txtVpd');
    const elVap = document.getElementById('kpiVaporPress');
    if (elVpd) elVpd.innerHTML = `${vpd.toFixed(3)} <span style="font-size:1rem;">kPa</span>`;
    if (elTxtVpd) elTxtVpd.innerText = `${vpd.toFixed(3)} kPa`;
    if (elVap) elVap.innerText = `Pressão Vapor: ${vaporPress.toFixed(1)} hPa`;

    // 9. Entalpia Específica
    const elEnth = document.getElementById('kpiEnthalpy');
    const elTxtEnth = document.getElementById('txtEnthalpy');
    if (elEnth) elEnth.innerHTML = `${enthalpy.toFixed(1)} <span style="font-size:0.9rem;">kJ/kg</span>`;
    if (elTxtEnth) elTxtEnth.innerText = `${enthalpy.toFixed(1)} kJ/kg`;

    // 10. Tensão e Resistência do MQ-135
    const elVolt = document.getElementById('kpiGasVolt');
    const elRs = document.getElementById('kpiGasRs');
    if (elVolt) elVolt.innerHTML = `${volt.toFixed(3)} <span style="font-size:1rem;">V</span>`;
    if (elRs) elRs.innerText = `Rs Carga: ${rs.toFixed(1)} kΩ`;

    // 11. Uptime
    const elUptime = document.getElementById('kpiUptime');
    if (elUptime && d.uptime_sec !== undefined) elUptime.innerText = `Uptime: ${d.uptime_sec}s`;

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