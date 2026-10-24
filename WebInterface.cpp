#include "WebInterface.h"
#include "Actuators.h"
#include "Sensors.h"
#include "Storage.h"
#include "Diagnostics.h"
#include "NetworkManager.h"
#include "TelegramBot.h"
#include "TelegramAdmin.h"
#include "AtalaiaModel.h"
#include "DisplayManager.h"
#include "images.h"

// ============================================================================
// DASHBOARD INDUSTRIAL SPA DARK
// ============================================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ATALAIA 3 — Painel Operacional</title>
  <style>
    :root {
      --bg: #090d16;
      --card: #111827;
      --panel: #0f172a;
      --border: #1e293b;
      --text: #f8fafc;
      --muted: #94a3b8;
      --accent: #38bdf8;
      --green: #10b981;
      --amber: #f59e0b;
      --red: #ef4444;
      --purple: #8b5cf6;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: ui-monospace, Menlo, Monaco, Consolas, "Courier New", monospace; background: var(--bg); color: var(--text); padding: 12px; font-size: 13px; line-height: 1.4; }
    .wrap { max-width: 1024px; margin: 0 auto; }
    .panel { background: var(--panel); border: 1px solid var(--border); border-radius: 8px; padding: 14px; margin-bottom: 12px; }
    h1 { font-size: 1.2rem; color: var(--accent); text-align: center; margin-bottom: 4px; letter-spacing: 1.2px; text-transform: uppercase; font-weight: 800; }
    .status-bar { text-align: center; font-size: 0.78rem; color: var(--muted); margin-bottom: 10px; }
    .badges { display: flex; flex-wrap: wrap; gap: 6px; justify-content: center; margin-top: 6px; }
    .badge { padding: 4px 8px; border-radius: 4px; font-size: 0.7rem; font-weight: 700; letter-spacing: 0.4px; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(145px, 1fr)); gap: 10px; margin-bottom: 12px; }
    .card { background: var(--card); border: 1px solid var(--border); padding: 10px; border-radius: 6px; text-align: center; }
    .card-t { font-size: 0.65rem; color: var(--muted); text-transform: uppercase; letter-spacing: 0.6px; font-weight: 600; }
    .card-v { font-size: 1.35rem; font-weight: 800; margin: 4px 0; }
    .card-s { font-size: 0.68rem; color: #64748b; }
    .btn-row { display: flex; flex-wrap: wrap; gap: 6px; justify-content: center; align-items: center; }
    button { background: #1e293b; color: #f8fafc; border: 1px solid #334155; padding: 7px 11px; border-radius: 4px; cursor: pointer; font-size: 0.72rem; font-family: inherit; font-weight: 700; transition: background 0.15s; }
    button:hover { background: #334155; }
    button.primary { background: #0284c7; border-color: var(--accent); }
    button.success { background: #047857; border-color: var(--green); }
    button.warning { background: #b45309; border-color: var(--amber); }
    button.danger  { background: #b91c1c; border-color: var(--red); }
    button.purple  { background: #6d28d9; border-color: var(--purple); }
    input, select { background: #020617; border: 1px solid #334155; color: #fff; padding: 6px 8px; border-radius: 4px; font-size: 0.72rem; font-family: inherit; }
    .log-box { max-height: 180px; overflow-y: auto; background: #020617; border: 1px solid var(--border); padding: 8px; font-size: 0.7rem; border-radius: 4px; }
    .log-item { border-bottom: 1px solid #0f172a; padding: 3px 0; display: flex; gap: 8px; }
    .toast { text-align: center; font-size: 0.75rem; color: var(--accent); min-height: 18px; margin-top: 8px; font-weight: 700; }
    canvas { width: 100%; height: 160px; background: #020617; border-radius: 4px; border: 1px solid var(--border); }
    .sec-t { font-size: 0.72rem; color: var(--muted); margin-bottom: 8px; text-transform: uppercase; font-weight: 700; letter-spacing: 0.5px; border-left: 3px solid var(--accent); padding-left: 6px; }
  </style>
</head>
<body>
<div class="wrap">
  <div class="panel">
    <h1>🛰️ ATALAIA 3 — ESTAÇÃO DE MONITORAMENTO</h1>
    <div class="status-bar">
      Local: <span id="loc" style="color:var(--accent);">(SEM LOCAL)</span> |
      Coord: <span id="coord" style="color:#a5f3fc;">sem local</span> |
      Relógio: <span id="clk">--:--:--</span> |
      IP: <span id="ipAddr" style="color:var(--accent); font-weight:bold;">192.168.4.1</span>
    </div>
    <div class="badges">
      <span id="bPower" class="badge" style="background:#047857;">OPERACIONAL</span>
      <span id="bRisk" class="badge" style="background:#047857;">RISCO: NORMAL</span>
      <span id="bML" class="badge" style="background:#0284c7;">TINYML: --</span>
      <span id="bScreen" class="badge" style="background:#6d28d9;">TELA: [1/4]</span>
      <span id="bNet" class="badge" style="background:#0284c7;">REDE: --</span>
      <span id="bWarm" class="badge" style="background:#b45309;">MQ: --</span>
      <span id="bSound" class="badge" style="background:#047857;">AUDIO: ON</span>
      <span id="bBeepDev" class="badge" style="background:#0284c7;">BIP: 5V</span>
      <span id="bSiren" class="badge" style="background:#047857;">SIRENES: ARMADAS</span>
      <span id="bLed" class="badge" style="background:#475569;">LED: AUTO</span>
    </div>
  </div>

  <div class="grid">
    <div class="card">
      <div class="card-t">Temperatura</div>
      <div class="card-v" id="tVal" style="color:var(--accent);">-- °C</div>
      <div class="card-s" id="hiVal">Sensação: --</div>
      <div class="card-s" id="rateVal">dT/dt: 0.0 °C/min</div>
    </div>
    <div class="card">
      <div class="card-t">Umidade</div>
      <div class="card-v" id="hVal" style="color:#60a5fa;">-- %</div>
      <div class="card-s" id="dpVal">Orvalho: --</div>
      <div class="card-s" id="qualVal">Qualidade: VÁLIDA</div>
    </div>
    <div class="card">
      <div class="card-t">Gás MQ-135 (Bruto / Cal)</div>
      <div class="card-v" id="gVal" style="color:var(--green);">ADC --</div>
      <div class="card-s" id="ppmVal">~-- ppm</div>
      <div class="card-s" id="aqiVal" style="color:var(--accent); font-weight:bold;">IQA: --</div>
    </div>
    <div class="card">
      <div class="card-t">Sensor de Chuva</div>
      <div class="card-v" id="rVal">--</div>
      <div class="card-s" id="rAdcVal">ADC: --</div>
    </div>
    <div class="card">
      <div class="card-t">Sensor de Fogo (D33)</div>
      <div class="card-v" id="fVal">--</div>
      <div class="card-s" id="fPolVal">Polaridade: LOW Ativo</div>
    </div>
    <div class="card">
      <div class="card-t">Buffer Flash (12h)</div>
      <div class="card-v" id="bufVal">-- / 8640</div>
      <div class="card-s" id="totVal">Lote RAM: 0/12</div>
    </div>
  </div>

  <!-- GERENCIAMENTO ESPELHADO DO DISPLAY OLED -->
  <div class="panel">
    <div class="sec-t">🖥️ Controle e Navegação do Display OLED (Espelhado com Telegram)</div>
    <div class="btn-row" style="margin-bottom:8px;">
      <button class="primary" onclick="reqAction('/api/display/prev')">◀ Tela Anterior</button>
      <button class="primary" onclick="reqAction('/api/display/next')">Próxima Tela ▶</button>
      <span style="font-size:0.75rem; font-weight:bold;">Ir para:</span>
      <select id="screenSelect" onchange="reqAction('/api/display/select?screen=' + this.value)">
        <option value="0">1. Dashboard Geral</option>
        <option value="1">2. Terminal CMD</option>
        <option value="2">3. Gráficos Históricos</option>
        <option value="3">4. Diagnóstico de Rede</option>
      </select>
      <button id="btnPin" class="warning" onclick="reqAction('/api/display/pin_toggle')">📌 Fixar Tela</button>
      <button class="purple" onclick="reqAction('/api/display/ip_toggle')">👁️ IP no Display: ON/OFF</button>
    </div>
    <div class="btn-row">
      <span style="font-size:0.75rem; font-weight:bold;">Projetar Imagem:</span>
      <select id="imgSelect"></select>
      <span style="font-size:0.75rem;">Tempo:</span>
      <input type="number" id="imgDuration" value="5" min="1" max="60" style="width:55px;"> seg
      <button class="success" onclick="showSelectedImage()">Exibir no OLED</button>
      <button class="danger" onclick="reqAction('/api/display/image_stop')">Interromper Imagem</button>
      <button class="purple" onclick="reqAction('/api/display/christmas')">🎄 Animação Natalícia</button>
    </div>
  </div>

  <div class="panel">
    <div class="sec-t">📈 Telemetria Temporal ao Vivo (Temp / Umid / Leitura MQ)</div>
    <canvas id="chartCanvas" width="940" height="160"></canvas>
  </div>

  <div class="panel">
    <div class="sec-t">🩺 Diagnóstico Avançado do Silício ESP32</div>
    <div class="grid" style="grid-template-columns: repeat(auto-fit, minmax(130px, 1fr)); margin-bottom: 0;">
      <div class="card">
        <div class="card-t">Temp Silício</div>
        <div class="card-v" id="cpuTempVal" style="color:#f87171;">-- °C</div>
        <div class="card-s" id="cpuFreqVal">Freq: -- MHz</div>
      </div>
      <div class="card">
        <div class="card-t">Heap Livre</div>
        <div class="card-v" id="heapVal" style="color:var(--accent);">-- kB</div>
        <div class="card-s" id="heapMinVal">Mín: -- kB</div>
      </div>
      <div class="card">
        <div class="card-t">Fragmentação</div>
        <div class="card-v" id="fragVal" style="color:var(--amber);">-- %</div>
        <div class="card-s" id="maxAllocVal">Bloco: -- kB</div>
      </div>
      <div class="card">
        <div class="card-t">Sinal Wi-Fi</div>
        <div class="card-v" id="rssiVal">-- dBm</div>
        <div class="card-s" id="qualWifiVal">Qualidade: -- %</div>
      </div>
      <div class="card" style="grid-column: span 2;">
        <div class="card-t">Causa do Último Boot</div>
        <div class="card-v" id="resetReasonVal" style="font-size:0.85rem; margin-top:8px; color:#e2e8f0;">--</div>
        <div class="card-s" id="uptimeFmtVal">Uptime: --</div>
      </div>
    </div>
  </div>

  <div class="panel">
    <div class="sec-t">⚙️ Gestão de Sirenes, Áudio e Calibração de Sensores</div>
    <div class="btn-row" style="margin-bottom:10px;">
      <span style="font-size:0.75rem;">Atuador Bip:</span>
      <select id="beepSelect" onchange="reqAction('/api/settings/beep_device?dev=' + this.value)">
        <option value="0">Buzzer 5V Ativo (GPIO 27)</option>
        <option value="1">Buzzer 3.3V (GPIO 18)</option>
        <option value="2">AMBOS OS BUZZERS (D18 + D27)</option>
      </select>
      <button class="primary" onclick="reqAction('/test?target=buzzer3v')">Bip 3.3V</button>
      <button onclick="reqAction('/test?target=buzzer5v')">Bip 5V</button>
      <button class="warning" onclick="reqAction('/test?target=all')">Bip Duplo</button>
    </div>
    <div class="btn-row" style="margin-bottom:10px;">
      <span style="font-size:0.75rem;">Limiar Gás:</span>
      <input type="number" id="gasPpmInput" min="50" max="5000" step="10" style="width:85px;"> ppm
      <button class="success" onclick="saveGasThreshold()">Gravar Limiar</button>
      <span style="font-size:0.75rem; margin-left:10px;">Polaridade Fogo (D33):</span>
      <button class="purple" onclick="reqAction('/api/sensors/flame_polarity_toggle')">Alternar Nível LOW/HIGH</button>
    </div>
    <div class="btn-row">
      <button onclick="reqAction('/api/settings/sound_master_toggle')">Som Mestre (ON/OFF)</button>
      <button onclick="reqAction('/api/settings/collection_beep_toggle')">Bip Coleta (ON/OFF)</button>
      <button onclick="reqAction('/api/settings/risk_sirens_toggle')">Sirenes de Risco (Armar/Desarmar)</button>
      <button onclick="reqAction('/api/settings/led_mode?mode=auto')">LED: Auto</button>
      <button onclick="reqAction('/api/settings/led_mode?mode=on')">LED: ON</button>
      <button onclick="reqAction('/api/settings/led_mode?mode=off')">LED: OFF</button>
    </div>
  </div>

  <div class="panel">
    <div class="sec-t"> Sirenes de Bancada, Exportação e Manutenção</div>
    <div class="btn-row">
      <button class="danger" onclick="reqAction('/test?target=buzzer3v_fire')">🚨 Sirene Fogo (3.5s)</button>
      <button class="warning" onclick="reqAction('/test?target=gas_alarm')">⚠️ Alarme Gás (3.5s)</button>
      <button class="success" onclick="reqAction('/test?target=buzzer3v_rain')">🎄 Melodia Natal (D18)</button>
      <button class="danger" onclick="reqAction('/test?target=stop_sound')">🛑 PARAR SONS</button>
      <button class="primary" onclick="reqAction('/api/settings/warmup?mode=fast')">Pular Warm-up</button>
      <button class="primary" onclick="window.open('/api/export.csv')">📊 Exportar CSV com Coordenadas</button>
      <button class="primary" onclick="window.open('/atalaia_buffer.bin')">💾 Backup .BIN</button>
      <button class="danger" onclick="reqAction('/test?target=reset_buffer','Zerar todo o histórico da Flash?')">Zerar Buffer</button>
      <button id="btnStandby" class="warning" onclick="toggleStandby()">Alternar Standby</button>
      <button class="primary" onclick="reqAction('/api/power/reboot','Reiniciar placa?')">Reiniciar ESP32</button>
    </div>
    <div class="toast" id="msgToast"></div>
  </div>

  <div class="panel">
    <div class="sec-t">📡 Conexão Wi-Fi</div>
    <div style="display:flex; flex-direction:column; gap:6px; max-width:380px; margin:0 auto;">
      <div style="display:flex; gap:4px;">
        <select id="wList" style="flex:1;"><option value="">Clique em Procurar Redes...</option></select>
        <button onclick="scanNet()">Procurar</button>
      </div>
      <input type="password" id="wPass" placeholder="Palavra-passe de Rede">
      <div class="btn-row">
        <button class="primary" onclick="saveNet()">Guardar e Ligar</button>
        <button class="warning" onclick="reqAction('/api/network/force_ap','Ativar modo AP local?')">Modo SoftAP</button>
        <button class="success" onclick="reqAction('/api/network/force_sta','Reconectar à rede Station guardada?')">Ligar Roteador</button>
        <button class="danger" onclick="reqAction('/api/wifi_reset','Esquecer rede configurada?')">Esquecer Rede</button>
      </div>
      <div id="wifiStatusBadge" style="display:none; text-align:center; padding:8px; border-radius:4px; font-weight:bold;"></div>
    </div>
  </div>

  <div class="panel">
    <div class="sec-t">📋 Registro de Eventos do Sistema</div>
    <div class="log-box" id="logBox">A carregar eventos...</div>
  </div>
</div>

<script>
  let currentStandby = false;
  let cachedIp = "192.168.4.1";
  const historyData = { temp: [], hum: [], mqRaw: [] };
  const MAX_POINTS = 30;

  const showMsg = (txt) => {
    const el = document.getElementById('msgToast');
    if (el) el.innerText = txt;
  };

  const reqAction = (url, promptMsg) => {
    if (promptMsg && !confirm(promptMsg)) return;
    fetch(url, { method: 'POST' })
      .then(r => r.json())
      .then(d => {
        showMsg(d.message || d.action || "Comando executado!");
        refreshData();
      })
      .catch(() => showMsg("Comando executado!"));
  };

  const showSelectedImage = () => {
    const idx = document.getElementById('imgSelect').value;
    const dur = parseInt(document.getElementById('imgDuration').value) || 5;
    reqAction('/api/display/image?index=' + idx + '&duration=' + (dur * 1000));
  };

  const saveGasThreshold = () => {
    const v = document.getElementById('gasPpmInput').value.trim();
    if (!v || isNaN(v)) return;
    fetch('/api/settings/gas_threshold?val=' + encodeURIComponent(v), { method: 'POST' })
      .then(r => r.json())
      .then(d => {
        showMsg("Limiar atualizado: " + d.gas_threshold + " ppm");
        refreshData();
      });
  };

  const toggleStandby = () => {
    reqAction('/api/power/standby?enable=' + (!currentStandby ? '1' : '0'));
  };

  const renderCanvasChart = () => {
    const cv = document.getElementById('chartCanvas');
    if (!cv) return;
    const ctx = cv.getContext('2d');
    const w = cv.width, h = cv.height;

    ctx.fillStyle = '#020617';
    ctx.fillRect(0, 0, w, h);

    ctx.strokeStyle = '#1e293b';
    ctx.lineWidth = 1;
    for (let x = 0; x < w; x += 40) { ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, h); ctx.stroke(); }
    for (let y = 0; y < h; y += 32) { ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke(); }

    if (historyData.temp.length < 2) return;

    const drawSeries = (arr, minVal, maxVal, color) => {
      ctx.strokeStyle = color;
      ctx.lineWidth = 2;
      ctx.beginPath();
      const step = w / (MAX_POINTS - 1);
      arr.forEach((val, idx) => {
        const norm = (val - minVal) / (maxVal - minVal || 1);
        const y = h - (norm * (h - 20) + 10);
        const x = idx * step;
        if (idx === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      });
      ctx.stroke();
    };

    drawSeries(historyData.temp, 10, 50, '#38bdf8');
    drawSeries(historyData.hum, 0, 100, '#60a5fa');
    drawSeries(historyData.mqRaw, 200, 3800, '#10b981');
  };

  const refreshData = () => {
    fetch('/api/current')
      .then(r => r.json())
      .then(d => {
        currentStandby = d.standby_mode;
        cachedIp = d.ip;
        document.getElementById('loc').innerText = d.location;
        document.getElementById('coord').innerText = d.latitude + ', ' + d.longitude;
        document.getElementById('clk').innerText = d.datetime;
        document.getElementById('ipAddr').innerText = d.ip;

        document.getElementById('tVal').innerText = d.temperature.toFixed(1) + ' °C';
        document.getElementById('hiVal').innerText = 'Sensação: ' + d.heat_index.toFixed(1) + ' °C';
        document.getElementById('rateVal').innerText = 'dT/dt: ' + (d.temp_rate >= 0 ? '+' : '') + d.temp_rate.toFixed(2) + ' °C/min';
        document.getElementById('hVal').innerText = d.humidity.toFixed(1) + ' %';
        document.getElementById('dpVal').innerText = 'Orvalho: ' + d.dew_point.toFixed(1) + ' °C';

        document.getElementById('gVal').innerText = 'ADC ' + d.mq135_raw;
        document.getElementById('ppmVal').innerText = '~' + Math.round(d.mq135_ppm_comp) + ' ppm';
        document.getElementById('aqiVal').innerText = 'IQA: ' + d.aqi + ' (' + d.air_quality + ')';

        const elR = document.getElementById('rVal');
        elR.innerText = d.rain_detected ? "DETECTADA" : "SECO";
        elR.style.color = d.rain_detected ? "#38bdf8" : "#10b981";
        document.getElementById('rAdcVal').innerText = 'ADC: ' + d.rain_raw;

        const elF = document.getElementById('fVal');
        elF.innerText = d.flame_detected ? "FOGO DETECTADO!" : "SEGURO";
        elF.style.color = d.flame_detected ? "#ef4444" : "#10b981";
        document.getElementById('fPolVal').innerText = 'Polaridade: ' + (d.flame_active_low ? 'LOW Ativo' : 'HIGH Ativo');

        document.getElementById('bufVal').innerText = d.head_index + " / 8640";
        document.getElementById('totVal').innerText = "Lote RAM: " + d.ram_pending + "/12";

        document.getElementById('screenSelect').value = String(d.current_screen);
        document.getElementById('bScreen').innerText = "TELA: [" + (d.current_screen + 1) + "/" + d.total_screens + "] " + (d.is_pinned ? "FIXA" : "ROTAÇÃO");

        const btnP = document.getElementById('btnPin');
        btnP.innerText = d.is_pinned ? "🔄 Liberar Carrossel" : "📌 Fixar Tela";

        const bPow = document.getElementById('bPower');
        bPow.innerText = d.standby_mode ? "STANDBY" : "OPERACIONAL";
        bPow.style.background = d.standby_mode ? "#b45309" : "#047857";

        const rColors = ["#047857", "#b45309", "#dc2626", "#7f1d1d"];
        const bRisk = document.getElementById('bRisk');
        bRisk.innerText = "RISCO: " + d.risk_name;
        bRisk.style.background = rColors[d.risk_level] || "#047857";

        const bML = document.getElementById('bML');
        bML.innerText = "TINYML: " + d.ml_state_name;
        bML.style.background = d.ml_color;

        const bNet = document.getElementById('bNet');
        bNet.innerText = d.mode + ": " + d.ssid + (d.show_ip ? " (" + d.ip + ")" : " (IP Oculto)");
        bNet.style.background = (d.mode === "AP") ? "#d97706" : "#047857";

        const bWarm = document.getElementById('bWarm');
        bWarm.innerText = d.warmed_up ? "MQ CALIBRADO" : ("MQ AQUECENDO (" + d.warmup_remaining_sec + "s)");
        bWarm.style.background = d.warmed_up ? "#047857" : "#b45309";

        historyData.temp.push(d.temperature);
        historyData.hum.push(d.humidity);
        historyData.mqRaw.push(d.mq135_raw);
        if (historyData.temp.length > MAX_POINTS) {
          historyData.temp.shift();
          historyData.hum.shift();
          historyData.mqRaw.shift();
        }
        renderCanvasChart();
      }).catch(() => {});
  };

  const loadImagesList = () => {
    fetch('/api/display/images_list')
      .then(r => r.json())
      .then(d => {
        const sel = document.getElementById('imgSelect');
        sel.innerHTML = '';
        d.images.forEach((img, i) => {
          const opt = document.createElement('option');
          opt.value = i;
          opt.innerText = (i + 1) + '. ' + img.name + ' (' + img.width + 'x' + img.height + ')';
          sel.appendChild(opt);
        });
      }).catch(() => {});
  };

  const refreshHealth = () => {
    fetch('/api/health')
      .then(r => r.json())
      .then(h => {
        document.getElementById('cpuTempVal').innerText = h.cpu_temp.toFixed(1) + ' °C';
        document.getElementById('cpuFreqVal').innerText = 'Freq: ' + h.cpu_freq + ' MHz';
        document.getElementById('heapVal').innerText = Math.round(h.free_heap / 1024) + ' kB';
        document.getElementById('heapMinVal').innerText = 'Mín: ' + Math.round(h.min_free_heap / 1024) + ' kB';
        document.getElementById('fragVal').innerText = h.fragmentation + ' %';
        document.getElementById('maxAllocVal').innerText = 'Bloco: ' + Math.round(h.max_alloc / 1024) + ' kB';
        document.getElementById('rssiVal').innerText = h.rssi + ' dBm';
        document.getElementById('qualWifiVal').innerText = 'Qualidade: ' + h.signal_quality + ' %';
        document.getElementById('resetReasonVal').innerText = h.reset_reason;

        const sec = h.uptime_sec;
        const hh = Math.floor(sec / 3600);
        const mm = Math.floor((sec % 3600) / 60);
        const ss = sec % 60;
        document.getElementById('uptimeFmtVal').innerText = 'Uptime: ' + hh + 'h ' + mm + 'm ' + ss + 's';
      }).catch(() => {});
  };

  const refreshLogs = () => {
    fetch('/api/logs')
      .then(r => r.json())
      .then(d => {
        const box = document.getElementById('logBox');
        if (!d.logs || !d.logs.length) {
          box.innerHTML = "<div>Nenhum evento registrado até o momento.</div>";
          return;
        }
        const colors = ["#94a3b8", "#f59e0b", "#ef4444"];
        box.innerHTML = d.logs.map(l =>
          `<div class="log-item" style="color:${colors[l.sev] || '#94a3b8'};">
            <span>[${l.time}]</span>
            <b>[${l.cat}]</b>
            <span>${l.msg}</span>
           </div>`
        ).join('');
      }).catch(() => {});
  };

  const scanNet = () => {
    const s = document.getElementById('wList');
    s.innerHTML = '<option>A procurar redes...</option>';
    fetch('/api/scan').then(r => r.json()).then(d => {
      s.innerHTML = '<option value="">Selecione um SSID...</option>';
      d.networks.forEach(n => {
        const opt = document.createElement('option');
        opt.value = n.ssid;
        opt.innerText = n.ssid + ' (' + n.rssi + ' dBm)';
        s.appendChild(opt);
      });
    });
  };

  const saveNet = () => {
    const ssid = document.getElementById('wList').value;
    const pass = document.getElementById('wPass').value;
    if (!ssid) return;
    fetch('/api/wifi_save', {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: 'ssid=' + encodeURIComponent(ssid) + '&pass=' + encodeURIComponent(pass)
    }).then(() => showMsg("Credenciais salvas. A reconectar..."));
  };

  setInterval(refreshData, 2000);
  setInterval(refreshHealth, 3000);
  setInterval(refreshLogs, 5000);
  refreshData();
  refreshHealth();
  refreshLogs();
  loadImagesList();
</script>
</body>
</html>
)rawliteral";

bool handleCaptivePortal() {
  if (!isApMode) return false;
  String host = server.hostHeader();
  if (!host.startsWith("192.168.4.1") && !host.startsWith("atalaia3.local")) {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/html", "<meta http-equiv='refresh' content='0;url=http://192.168.4.1/'><a href='http://192.168.4.1/'>Login</a>");
    return true;
  }
  return false;
}

void setupWebServer() {
  server.enableCORS(true);

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", INDEX_HTML);
  });

  auto captiveRedirect = []() {
    if (isApMode) {
      server.sendHeader("Location", "http://192.168.4.1/", true);
      server.send(302, "text/html", "<meta http-equiv='refresh' content='0;url=http://192.168.4.1/'><a href='http://192.168.4.1/'>Painel</a>");
    } else {
      server.send(204, "text/plain", "");
    }
  };

  server.on("/generate_204", HTTP_GET, captiveRedirect);
  server.on("/gen_204", HTTP_GET, captiveRedirect);
  server.on("/hotspot-detect.html", HTTP_GET, captiveRedirect);
  server.on("/canonical.html", HTTP_GET, captiveRedirect);
  server.on("/ncsi.txt", HTTP_GET, captiveRedirect);
  server.on("/connecttest.txt", HTTP_GET, captiveRedirect);
  server.on("/redirect", HTTP_GET, captiveRedirect);
  server.on("/success.txt", HTTP_GET, captiveRedirect);

  server.on("/api/current", HTTP_GET, []() {
    unsigned long now = millis();
    bool warmedUp = (now >= warmupDurationMs);
    uint32_t rem = warmedUp ? 0 : (uint32_t)((warmupDurationMs - now) / 1000UL);
    int currentAqi = calculateAQI(currentData.mq135_ppm_comp);

    String ipStr = isApMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    if ((ipStr == "0.0.0.0" || ipStr.length() == 0) && isApMode) {
      ipStr = "192.168.4.1";
    }

    String json = "{";
    json += "\"temperature\":" + String(currentData.temperature, 1) + ",";
    json += "\"humidity\":" + String(currentData.humidity, 1) + ",";
    json += "\"heat_index\":" + String(currentData.heat_index, 1) + ",";
    json += "\"dew_point\":" + String(currentData.dew_point, 1) + ",";
    json += "\"temp_rate\":" + String(currentData.temp_rate_minute, 2) + ",";
    json += "\"mq135_raw\":" + String(currentData.mq135_raw) + ",";
    json += "\"mq135_ppm_comp\":" + String(currentData.mq135_ppm_comp, 1) + ",";
    json += "\"aqi\":" + String(currentAqi) + ",";
    json += "\"air_quality\":\"" + getAQICategory(currentAqi) + "\",";
    json += "\"rain_detected\":" + String(currentData.rain_detected ? "true" : "false") + ",";
    json += "\"rain_raw\":" + String(currentData.rain_raw) + ",";
    json += "\"flame_detected\":" + String(currentData.flame_detected ? "true" : "false") + ",";
    json += "\"flame_active_low\":" + String(flameActiveLow ? "true" : "false") + ",";
    json += "\"data_quality\":" + String((int)currentData.data_quality) + ",";
    json += "\"risk_level\":" + String((int)currentData.risk_level) + ",";
    json += "\"risk_name\":\"" + String(getRiskLevelName((RiskLevel)currentData.risk_level)) + "\",";
    json += "\"ml_state\":" + String((int)currentData.ml_state) + ",";
    json += "\"ml_state_name\":\"" + String(AtalaiaModel::getStateName((MLState)currentData.ml_state)) + "\",";
    json += "\"ml_color\":\"" + String(AtalaiaModel::getStateBadgeColor((MLState)currentData.ml_state)) + "\",";
    json += "\"uptime_sec\":" + String(now / 1000UL) + ",";
    json += "\"session_id\":" + String(currentSessionId) + ",";
    json += "\"head_index\":" + String(ringMeta.head_index) + ",";
    json += "\"total_saved\":" + String(ringMeta.total_saved) + ",";
    json += "\"ram_pending\":" + String(getPendingBatchCount()) + ",";
    json += "\"mode\":\"" + String(isApMode ? "AP" : "STA") + "\",";
    json += "\"ssid\":\"" + String(isApMode ? AP_SSID : WiFi.SSID()) + "\",";
    json += "\"rssi\":" + String(isApMode ? 0 : WiFi.RSSI()) + ",";
    json += "\"ip\":\"" + ipStr + "\",";
    json += "\"show_ip\":" + String(showIpOnDisplay ? "true" : "false") + ",";
    json += "\"current_screen\":" + String(getCurrentDisplayScreen()) + ",";
    json += "\"total_screens\":" + String(getTotalDisplayScreens()) + ",";
    json += "\"is_pinned\":" + String(isDisplayPinned() ? "true" : "false") + ",";
    json += "\"standby_mode\":" + String(isSystemStandby ? "true" : "false") + ",";
    json += "\"sound_master\":" + String(masterSoundEnabled ? "true" : "false") + ",";
    json += "\"collection_beep\":" + String(collectionBeepEnabled ? "true" : "false") + ",";
    json += "\"beep_device\":" + String((int)selectedBeepDevice) + ",";
    json += "\"gas_threshold\":" + String(gasAlertThresholdPpm, 1) + ",";
    json += "\"risk_sirens\":" + String(riskSirensEnabled ? "true" : "false") + ",";
    json += "\"led_mode\":" + String((int)currentLedMode) + ",";
    json += "\"location\":\"" + locationInfo + "\",";
    json += "\"latitude\":\"" + locationLatitude + "\",";
    json += "\"longitude\":\"" + locationLongitude + "\",";
    json += "\"datetime\":\"" + getFormattedDateTime() + "\",";
    json += "\"warmed_up\":" + String(warmedUp ? "true" : "false") + ",";
    json += "\"warmup_remaining_sec\":" + String(rem);
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/api/display/next", HTTP_POST, []() {
    nextDisplayScreen();
    server.send(200, "application/json", "{\"status\":\"ok\",\"action\":\"next_screen\",\"current_screen\":" + String(getCurrentDisplayScreen()) + "}");
  });

  server.on("/api/display/prev", HTTP_POST, []() {
    previousDisplayScreen();
    server.send(200, "application/json", "{\"status\":\"ok\",\"action\":\"prev_screen\",\"current_screen\":" + String(getCurrentDisplayScreen()) + "}");
  });

  server.on("/api/display/select", HTTP_POST, []() {
    if (server.hasArg("screen")) {
      uint8_t sc = (uint8_t)server.arg("screen").toInt();
      selectDisplayScreen(sc);
      server.send(200, "application/json", "{\"status\":\"ok\",\"current_screen\":" + String(getCurrentDisplayScreen()) + "}");
      return;
    }
    server.send(400, "application/json", "{\"status\":\"error\"}");
  });

  server.on("/api/display/pin_toggle", HTTP_POST, []() {
    toggleDisplayPin();
    server.send(200, "application/json", "{\"status\":\"ok\",\"is_pinned\":" + String(isDisplayPinned() ? "true" : "false") + "}");
  });

  server.on("/api/display/ip_toggle", HTTP_POST, []() {
    showIpOnDisplay = !showIpOnDisplay;
    saveAudioAndPowerConfig();
    server.send(200, "application/json", "{\"status\":\"ok\",\"show_ip\":" + String(showIpOnDisplay ? "true" : "false") + "}");
  });

  server.on("/api/display/images_list", HTTP_GET, []() {
    String json = "{\"images\":[";
    for (uint8_t i = 0; i < SYSTEM_IMAGE_COUNT; i++) {
      if (i > 0) json += ",";
      json += "{\"name\":\"" + String(SYSTEM_IMAGES[i].name) + "\",";
      json += "\"width\":" + String(SYSTEM_IMAGES[i].width) + ",";
      json += "\"height\":" + String(SYSTEM_IMAGES[i].height) + "}";
    }
    json += "]}";
    server.send(200, "application/json", json);
  });

  server.on("/api/display/image", HTTP_POST, []() {
    uint8_t idx = 0;
    unsigned long dur = 5000;
    if (server.hasArg("index")) idx = (uint8_t)server.arg("index").toInt();
    if (server.hasArg("duration")) dur = (unsigned long)server.arg("duration").toInt();
    triggerDisplayCustomImage(idx, dur);
    server.send(200, "application/json", "{\"status\":\"ok\",\"image_index\":" + String(idx) + ",\"duration_ms\":" + String(dur) + "}");
  });

  server.on("/api/display/image_stop", HTTP_POST, []() {
    stopDisplayCustomImage();
    server.send(200, "application/json", "{\"status\":\"ok\",\"action\":\"image_stopped\"}");
  });

  server.on("/api/display/christmas", HTTP_POST, []() {
    triggerDisplayChristmasAnimation(8000);
    server.send(200, "application/json", "{\"status\":\"ok\",\"action\":\"christmas_animation\"}");
  });

  server.on("/api/sensors/flame_polarity_toggle", HTTP_POST, []() {
    flameActiveLow = !flameActiveLow;
    saveAudioAndPowerConfig();
    addEventLog("SENSOR", "Polaridade de fogo invertida para: " + String(flameActiveLow ? "LOW Ativo" : "HIGH Ativo"), 0);
    server.send(200, "application/json", "{\"status\":\"ok\",\"flame_active_low\":" + String(flameActiveLow ? "true" : "false") + "}");
  });

  server.on("/api/health", HTTP_GET, []() {
    SystemHealth h = getSystemHealth();
    String json = "{";
    json += "\"free_heap\":" + String(h.freeHeap) + ",";
    json += "\"min_free_heap\":" + String(h.minFreeHeap) + ",";
    json += "\"max_alloc\":" + String(h.maxAllocHeap) + ",";
    json += "\"fragmentation\":" + String(h.heapFragmentation) + ",";
    json += "\"cpu_temp\":" + String(h.cpuTemperature, 1) + ",";
    json += "\"cpu_freq\":" + String(h.cpuFreqMHz) + ",";
    json += "\"uptime_sec\":" + String(h.uptimeSeconds) + ",";
    json += "\"rssi\":" + String(h.wifiRssi) + ",";
    json += "\"signal_quality\":" + String(h.wifiSignalQuality) + ",";
    json += "\"reset_code\":" + String((int)h.resetCode) + ",";
    json += "\"reset_reason\":\"" + h.resetReason + "\"";
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/api/logs", HTTP_GET, []() {
    String json = "{\"logs\":[";
    int countToShow = min(eventLogCount, 16);
    for (int i = 0; i < countToShow; i++) {
      if (i > 0) json += ",";
      int idx = (eventLogHead - 1 - i + MAX_EVENT_LOGS) % MAX_EVENT_LOGS;
      json += "{\"time\":\"" + eventLogs[idx].timeStr + "\",";
      json += "\"cat\":\"" + eventLogs[idx].category + "\",";
      json += "\"sev\":" + String(eventLogs[idx].severity) + ",";
      json += "\"msg\":\"" + eventLogs[idx].message + "\"}";
    }
    json += "]}";
    server.send(200, "application/json", json);
  });

  server.on("/api/export.csv", HTTP_GET, []() {
    WiFiClient client = server.client();
    server.sendHeader("Content-Type", "text/csv");
    server.sendHeader("Content-Disposition", "attachment; filename=atalaia3_telemetria.csv");
    server.sendHeader("Connection", "close");
    server.send(200);
    exportStorageAsCsv(client);
  });

  server.on("/atalaia_buffer.bin", HTTP_GET, []() {
    flushRamBatch();
    if (LittleFS.exists(FILE_BUFFER)) {
      File f = LittleFS.open(FILE_BUFFER, FILE_READ);
      server.streamFile(f, "application/octet-stream");
      f.close();
    } else {
      server.send(404, "text/plain", "Buffer vazio");
    }
  });

  server.on("/api/scan", HTTP_GET, []() {
    int n = WiFi.scanNetworks();
    String json = "{\"networks\":[";
    for (int i = 0; i < n; ++i) {
      if (i > 0) json += ",";
      json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "]}";
    WiFi.scanDelete();
    server.send(200, "application/json", json);
  });

  server.on("/api/wifi_save", HTTP_POST, []() {
    if (server.hasArg("ssid")) {
      String ssid = server.arg("ssid");
      String pass = server.hasArg("pass") ? server.arg("pass") : "";
      saveWifiCredentials(ssid, pass);
      server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Credenciais salvas. A estabelecer ligação...\"}");
      pendingWifiReconnect = true;
      wifiReconnectTimer = millis() + 500;
      return;
    }
    server.send(400, "application/json", "{\"status\":\"error\"}");
  });

  server.on("/api/wifi_status", HTTP_GET, []() {
    bool connected = (WiFi.status() == WL_CONNECTED);
    String ipStr = connected ? WiFi.localIP().toString() : "";
    String json = "{\"connected\":" + String(connected ? "true" : "false") + ",\"ip\":\"" + ipStr + "\"}";
    server.send(200, "application/json", json);
  });

  server.on("/api/wifi_reset", HTTP_POST, []() {
    clearWifiCredentials();
    server.send(200, "application/json", "{\"status\":\"ok\"}");
    pendingWifiReconnect = true;
    wifiReconnectTimer = millis() + 500;
  });

  server.on("/api/network/force_ap", HTTP_POST, []() {
    server.send(200, "application/json", "{\"status\":\"ok\",\"action\":\"ap_activated\"}");
    delay(150);
    startAccessPoint();
  });

  server.on("/api/network/force_sta", HTTP_POST, []() {
    server.send(200, "application/json", "{\"status\":\"ok\",\"action\":\"sta_activated\"}");
    delay(150);
    setupNetworkStack();
  });

  server.on("/api/power/standby", HTTP_POST, []() {
    bool target = !isSystemStandby;
    if (server.hasArg("enable")) {
      target = (server.arg("enable") == "1" || server.arg("enable") == "true");
    }
    setSystemStandby(target, true);
    server.send(200, "application/json", "{\"status\":\"ok\",\"standby_mode\":" + String(isSystemStandby ? "true" : "false") + "}");
  });

  server.on("/api/power/reboot", HTTP_POST, []() {
    flushRamBatch();
    saveMetadataNow();
    server.send(200, "application/json", "{\"status\":\"ok\",\"action\":\"rebooting\"}");
    delay(300);
    ESP.restart();
  });

  server.on("/api/power/sleep", HTTP_POST, []() {
    server.send(200, "application/json", "{\"status\":\"ok\",\"action\":\"sleeping\"}");
    delay(300);
    enterSafeDeepSleep();
  });

  server.on("/api/settings/sound_master_toggle", HTTP_POST, []() {
    masterSoundEnabled = !masterSoundEnabled;
    saveAudioAndPowerConfig();
    server.send(200, "application/json", "{\"status\":\"ok\",\"sound_master\":" + String(masterSoundEnabled ? "true" : "false") + "}");
  });

  server.on("/api/settings/collection_beep_toggle", HTTP_POST, []() {
    collectionBeepEnabled = !collectionBeepEnabled;
    saveAudioAndPowerConfig();
    server.send(200, "application/json", "{\"status\":\"ok\",\"collection_beep\":" + String(collectionBeepEnabled ? "true" : "false") + "}");
  });

  server.on("/api/settings/beep_device", HTTP_POST, []() {
    if (server.hasArg("dev")) {
      int d = server.arg("dev").toInt();
      selectedBeepDevice = (BeepDevice)constrain(d, 0, 2);
      saveAudioAndPowerConfig();
      server.send(200, "application/json", "{\"status\":\"ok\",\"beep_device\":" + String((int)selectedBeepDevice) + "}");
      return;
    }
    server.send(400, "application/json", "{\"status\":\"error\"}");
  });

  server.on("/api/settings/gas_threshold", HTTP_POST, []() {
    if (server.hasArg("val")) {
      float val = server.arg("val").toFloat();
      if (val >= GAS_PPM_MIN_ALLOWED && val <= GAS_PPM_MAX_ALLOWED) {
        gasAlertThresholdPpm = val;
        saveAudioAndPowerConfig();
        server.send(200, "application/json", "{\"status\":\"ok\",\"gas_threshold\":" + String(gasAlertThresholdPpm, 1) + "}");
        return;
      }
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Limiar fora da faixa permitida (50 a 5000)\"}");
      return;
    }
    server.send(400, "application/json", "{\"status\":\"error\"}");
  });

  server.on("/api/settings/risk_sirens_toggle", HTTP_POST, []() {
    riskSirensEnabled = !riskSirensEnabled;
    if (!riskSirensEnabled) {
      stopSiren();
      stopGasAlarm();
      stopRainMelody();
    }
    saveAudioAndPowerConfig();
    server.send(200, "application/json", "{\"status\":\"ok\",\"risk_sirens\":" + String(riskSirensEnabled ? "true" : "false") + "}");
  });

  server.on("/api/settings/led_mode", HTTP_POST, []() {
    if (server.hasArg("mode")) {
      String m = server.arg("mode");
      if (m == "on") setLedMode(LED_MODE_FORCE_ON);
      else if (m == "off") setLedMode(LED_MODE_FORCE_OFF);
      else setLedMode(LED_MODE_AUTO_HEARTBEAT);
      saveAudioAndPowerConfig();
      server.send(200, "application/json", "{\"status\":\"ok\",\"led_mode\":" + String((int)currentLedMode) + "}");
      return;
    }
    server.send(400, "application/json", "{\"status\":\"error\"}");
  });

  server.on("/api/settings/warmup", HTTP_POST, []() {
    if (server.hasArg("mode")) {
      String m = server.arg("mode");
      if (m == "fast") {
        warmupDurationMs = 0;
        addEventLog("SENSOR", "Warm-up do MQ-135 suspenso manualmente.", 1);
      } else {
        warmupDurationMs = DEFAULT_WARMUP_MS;
        addEventLog("SENSOR", "Warm-up do MQ-135 restaurado (5 min).", 0);
      }
      server.send(200, "application/json", "{\"status\":\"ok\",\"warmup_ms\":" + String(warmupDurationMs) + "}");
      return;
    }
    server.send(400, "application/json", "{\"status\":\"error\"}");
  });

  auto handleTest = []() {
    if (server.hasArg("target")) {
      String target = server.arg("target");
      String feedbackMsg = "Executado: " + target;

      if (target == "buzzer5v") {
        triggerManualBeepTest(BEEP_DEV_BUZZER_5V, 45);
        feedbackMsg = "Bip curto no Buzzer 5V (D27) acionado!";
      } else if (target == "buzzer3v") {
        triggerManualBeepTest(BEEP_DEV_BUZZER_3V, 40);
        feedbackMsg = "Bip curto no Buzzer 3.3V (D18) acionado!";
      } else if (target == "all") {
        triggerManualBeepTest(BEEP_DEV_ALL, 50);
        feedbackMsg = "Bip duplo rápido (D18 + D27) acionado!";
      } else if (target == "buzzer3v_fire") {
        startSiren(true, 3500);
        feedbackMsg = "Sirene de Incêndio Louca acionada!";
      } else if (target == "gas_alarm") {
        startGasAlarm(true, 3500);
        feedbackMsg = "Sirene de Risco de Gás acionada!";
      } else if (target == "buzzer3v_rain") {
        startRainMelody(true);
        feedbackMsg = "Melodia Natalícia acionada no Buzzer 3.3V!";
      } else if (target == "stop_sound") {
        stopAllSounds();
        feedbackMsg = "Todos os sons foram interrompidos imediatamente.";
      } else if (target == "reset_buffer") {
        resetBufferStorage();
        feedbackMsg = "Buffer LittleFS de 12 horas zerado!";
      }

      server.send(200, "application/json", "{\"status\":\"ok\",\"action\":\"" + target + "\",\"message\":\"" + feedbackMsg + "\"}");
      return;
    }
    server.send(400, "application/json", "{\"status\":\"error\"}");
  };

  server.on("/test", HTTP_GET, handleTest);
  server.on("/test", HTTP_POST, handleTest);

  server.onNotFound([]() {
    if (isApMode) {
      server.sendHeader("Location", "http://192.168.4.1/", true);
      server.send(302, "text/html", "<meta http-equiv='refresh' content='0;url=http://192.168.4.1/'><a href='http://192.168.4.1/'>Painel</a>");
      return;
    }
    server.send(404, "text/plain", "Recurso não encontrado");
  });

  server.begin();
}