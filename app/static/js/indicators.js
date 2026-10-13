/**
 * Cálculos locais de apoio e atualização visual dos KPIs
 */
const Indicators = {
  updateCards(d) {
    if (!d) return;

    // 1. Valores Básicos
    document.getElementById('kpiTemp').innerHTML = `${Number(d.temperature).toFixed(1)} <span class="text-sm">°C</span>`;
    document.getElementById('kpiHum').innerHTML = `${Number(d.humidity).toFixed(1)} <span class="text-sm">%</span>`;
    document.getElementById('kpiGasRaw').innerText = d.mq135_raw || '--';

    // 2. Indicadores Derivados
    if (d.heat_index !== undefined) {
      document.getElementById('kpiHeatIndex').innerHTML = `${Number(d.heat_index).toFixed(1)} <span class="text-sm">°C</span>`;
    }
    if (d.dew_point !== undefined) {
      document.getElementById('kpiDewPoint').innerText = `Orvalho: ${Number(d.dew_point).toFixed(1)} °C`;
    }
    if (d.absolute_humidity_g_m3 !== undefined) {
      document.getElementById('kpiAbsHum').innerText = `Abs: ${Number(d.absolute_humidity_g_m3).toFixed(1)} g/m³`;
    }
    if (d.vapor_pressure_hpa !== undefined) {
      document.getElementById('kpiVaporPress').innerText = `Pressão: ${Number(d.vapor_pressure_hpa).toFixed(1)} hPa`;
    }
    if (d.estimated_ppm !== undefined) {
      document.getElementById('kpiGasPpm').innerText = `PPM: ~${Number(d.estimated_ppm).toFixed(0)} ppm`;
    }

    // 3. Chuva
    const elRain = document.getElementById('kpiRain');
    const isRain = Boolean(d.rain_detected);
    elRain.innerText = isRain ? "DETECTADA" : "SECO";
    elRain.className = `kpi-value font-extrabold ${isRain ? "text-cyan-400 drop-shadow-[0_0_10px_#22d3ee]" : "text-slate-400"}`;

    // 4. Fogo / Incêndio
    const elFlame = document.getElementById('kpiFlame');
    const isFlame = Boolean(d.flame_detected);
    if (isFlame) {
      elFlame.innerText = "EMERGÊNCIA!";
      elFlame.className = "kpi-value font-black text-rose-500 animate-pulse drop-shadow-[0_0_15px_#f43f5e]";
    } else {
      elFlame.innerText = "SEGURO";
      elFlame.className = "kpi-value font-extrabold text-emerald-400";
    }

    // 5. Gauge de Risco Integrado
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

    // Perímetro do raio 40: 2 * PI * 40 ~ 251.2
    const totalCircumference = 251.2;
    const offset = totalCircumference - (score / 100) * totalCircumference;
    circle.style.strokeDashoffset = offset;
    valText.innerText = score;

    if (score >= 60) {
      lblText.innerText = "PERIGO CRÍTICO";
      lblText.className = "text-xs font-bold text-rose-500 animate-bounce";
    } else if (score >= 30) {
      lblText.innerText = "ALERTA";
      lblText.className = "text-xs font-bold text-amber-400";
    } else {
      lblText.innerText = "NORMAL";
      lblText.className = "text-xs font-bold text-emerald-400";
    }
  }
};