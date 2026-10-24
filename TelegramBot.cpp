#include "TelegramBot.h"
#include "Actuators.h"
#include "Sensors.h"
#include "Storage.h"
#include "Diagnostics.h"
#include "NetworkManager.h"
#include "TelegramAdmin.h"
#include "AtalaiaModel.h"
#include "UnicodeGraphics.h"
#include "DisplayManager.h"
#include "images.h"
#include <LittleFS.h>

static bool botInitialized = false;
static SemaphoreHandle_t telegramTlsMutex = NULL;

static void ensureTelegramMutex() {
  if (telegramTlsMutex == NULL) {
    telegramTlsMutex = xSemaphoreCreateRecursiveMutex();
  }
}

bool acquireTelegramLock(TickType_t waitTicks) {
  ensureTelegramMutex();
  if (telegramTlsMutex == NULL) return false;
  return (xSemaphoreTakeRecursive(telegramTlsMutex, waitTicks) == pdTRUE);
}

void releaseTelegramLock() {
  if (telegramTlsMutex != NULL) {
    xSemaphoreGiveRecursive(telegramTlsMutex);
  }
}

static bool checkTlsHeapAvailability() {
  if (ESP.getFreeHeap() < 22000) {
    Serial.printf("[TLS-GUARD] Memoria Heap livre critica: %u B\n", ESP.getFreeHeap());
    return false;
  }
  return true;
}

static String jsonEscape(const String& str) {
  String out = "";
  out.reserve(str.length() + 64);
  for (size_t i = 0; i < str.length(); i++) {
    char c = str[i];
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else out += c;
  }
  return out;
}

bool initTelegramBot() {
  if (botInitialized) return true;
  if (!isStationConnected()) return false;

  Serial.println("[TELEGRAM-INIT] Validando conexao com Telegram...");
  ensureTelegramMutex();

  if (testTelegramApiConnection()) {
    botInitialized = true;
    Serial.println("[TELEGRAM-INIT] Bot homologado! Polling getUpdates LIBERADO.");
    addEventLog("TELEGRAM", "Pilha TLS do bot operacional.", 0);
    return true;
  }

  Serial.println("[TELEGRAM-INIT] Falha ao contatar Telegram API.");
  return false;
}

void notifyOledTelegramTask(const String& cmdName, const String& taskDescription) {
  triggerDisplayTaskPopup(cmdName, taskDescription, OLED_POPUP_HOLD_MS);
}

static String maskIpAddress(const String& ip) {
  if (ip.length() == 0 || ip == "0.0.0.0") return ip;
  int firstDot = ip.indexOf('.');
  int secondDot = ip.indexOf('.', firstDot + 1);
  if (firstDot != -1 && secondDot != -1) {
    return ip.substring(0, secondDot) + ".***.***";
  }
  return "***.***.***.***";
}

static String maskMacAddress(const String& mac) {
  if (mac.length() >= 17) {
    return mac.substring(0, 9) + ":XX:XX:XX";
  }
  return "**:**:**:**:**:**";
}

bool sendTelegramMessage(const String& text, bool priorityAlarm) {
  if (WiFi.status() != WL_CONNECTED || WiFi.localIP()[0] == 0) return false;

  if (!checkTlsHeapAvailability()) {
    telegramFailCount++;
    return false;
  }

  if (!acquireTelegramLock(pdMS_TO_TICKS(6000))) {
    telegramFailCount++;
    return false;
  }

  bool success = false;
  {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(6000);

    if (!client.connect("api.telegram.org", 443)) {
      telegramFailCount++;
      Serial.println("[TELEGRAM-SEND] Falha ao abrir socket TLS de envio.");
      releaseTelegramLock();
      return false;
    }

    String escapedText = jsonEscape(text);
    String payload = "";
    payload.reserve(escapedText.length() + 128);
    payload += "{\"chat_id\":";
    payload += String((long long)AUTHORIZED_CHAT_ID);
    payload += ",\"text\":\"";
    payload += escapedText;
    payload += "\",\"parse_mode\":\"HTML\"";
    if (priorityAlarm) {
      payload += ",\"disable_notification\":false}";
    } else {
      payload += ",\"disable_notification\":true}";
    }

    String postReq = "";
    postReq.reserve(payload.length() + 256);
    postReq += "POST /bot";
    postReq += String(TELEGRAM_BOT_TOKEN);
    postReq += "/sendMessage HTTP/1.1\r\n";
    postReq += "Host: api.telegram.org\r\n";
    postReq += "Content-Type: application/json; charset=utf-8\r\n";
    postReq += "Content-Length: ";
    postReq += String(payload.length());
    postReq += "\r\nConnection: close\r\n\r\n";
    postReq += payload;

    client.print(postReq);

    unsigned long t = millis();
    while (client.connected() && (millis() - t < 5000)) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        if (line.startsWith("HTTP/1.1 200 OK") || line.indexOf("\"ok\":true") != -1) {
          success = true;
          telegramSentCount++;
          break;
        }
      } else {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }
    client.stop();
  }

  releaseTelegramLock();

  if (!success) {
    telegramFailCount++;
    Serial.println("[TELEGRAM-SEND] Falha ao entregar mensagem.");
  } else {
    Serial.println("[TELEGRAM-SEND] Mensagem entregue com sucesso!");
  }

  return success;
}

bool sendTelegramDocument(const String& filePath, const String& caption) {
  if (WiFi.status() != WL_CONNECTED || WiFi.localIP()[0] == 0) return false;

  flushRamBatch();

  if (!LittleFS.exists(filePath)) {
    sendTelegramMessage("❌ <b>Erro:</b> Arquivo binário não encontrado no LittleFS.");
    return false;
  }

  File file = LittleFS.open(filePath, FILE_READ);
  if (!file || file.size() == 0) {
    if (file) file.close();
    sendTelegramMessage("⚠️ <b>Aviso:</b> O banco de dados está vazio.");
    return false;
  }

  if (!checkTlsHeapAvailability()) {
    file.close();
    return false;
  }

  if (!acquireTelegramLock(pdMS_TO_TICKS(8000))) {
    file.close();
    return false;
  }

  bool ok = false;
  {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(8000);

    if (!client.connect("api.telegram.org", 443)) {
      file.close();
      releaseTelegramLock();
      return false;
    }

    String boundary = "----WebKitFormBoundaryAtalaia3Edge";
    String fileName = "atalaia_buffer.bin";
    if (filePath.lastIndexOf('/') != -1) {
      fileName = filePath.substring(filePath.lastIndexOf('/') + 1);
    }

    String bodyHeader = "--" + boundary + "\r\n" +
                        "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" +
                        String((long long)AUTHORIZED_CHAT_ID) + "\r\n" +
                        "--" + boundary + "\r\n" +
                        "Content-Disposition: form-data; name=\"caption\"\r\n\r\n" +
                        caption + "\r\n" +
                        "--" + boundary + "\r\n" +
                        "Content-Disposition: form-data; name=\"document\"; filename=\"" + fileName + "\"\r\n" +
                        "Content-Type: application/octet-stream\r\n\r\n";

    String bodyFooter = "\r\n--" + boundary + "--\r\n";
    size_t totalLen = bodyHeader.length() + file.size() + bodyFooter.length();

    client.print("POST /bot" + String(TELEGRAM_BOT_TOKEN) + "/sendDocument HTTP/1.1\r\n" +
                 "Host: api.telegram.org\r\n" +
                 "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n" +
                 "Content-Length: " + String(totalLen) + "\r\n" +
                 "Connection: close\r\n\r\n");

    client.print(bodyHeader);

    uint8_t buf[512];
    while (file.available()) {
      size_t r = file.read(buf, sizeof(buf));
      client.write(buf, r);
    }
    file.close();

    client.print(bodyFooter);

    unsigned long t = millis();
    while (client.connected() && (millis() - t < 7000)) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        if (line.startsWith("HTTP/1.1 200 OK") || line.indexOf("\"ok\":true") != -1) {
          ok = true;
          break;
        }
      } else {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }
    client.stop();
  }

  releaseTelegramLock();
  return ok;
}

bool sendTelegramCsvData(const String& caption) {
  if (WiFi.status() != WL_CONNECTED || WiFi.localIP()[0] == 0) return false;

  flushRamBatch();

  if (!checkTlsHeapAvailability()) return false;
  if (!acquireTelegramLock(pdMS_TO_TICKS(8000))) return false;

  bool ok = false;
  {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(8000);

    if (!client.connect("api.telegram.org", 443)) {
      releaseTelegramLock();
      return false;
    }

    class SizeCounter : public Print {
    public:
      size_t count = 0;
      size_t write(uint8_t) override { count++; return 1; }
      size_t write(const uint8_t *buffer, size_t size) override { count += size; return size; }
    } counter;

    exportStorageAsCsv(counter);

    String boundary = "----WebKitFormBoundaryAtalaiaCsv";
    String bodyHeader = "--" + boundary + "\r\n" +
                        "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" +
                        String((long long)AUTHORIZED_CHAT_ID) + "\r\n" +
                        "--" + boundary + "\r\n" +
                        "Content-Disposition: form-data; name=\"caption\"\r\n\r\n" +
                        caption + "\r\n" +
                        "--" + boundary + "\r\n" +
                        "Content-Disposition: form-data; name=\"document\"; filename=\"atalaia_telemetria.csv\"\r\n" +
                        "Content-Type: text/csv\r\n\r\n";

    String bodyFooter = "\r\n--" + boundary + "--\r\n";
    size_t totalLen = bodyHeader.length() + counter.count + bodyFooter.length();

    client.print("POST /bot" + String(TELEGRAM_BOT_TOKEN) + "/sendDocument HTTP/1.1\r\n" +
                 "Host: api.telegram.org\r\n" +
                 "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n" +
                 "Content-Length: " + String(totalLen) + "\r\n" +
                 "Connection: close\r\n\r\n");

    client.print(bodyHeader);
    exportStorageAsCsv(client);
    client.print(bodyFooter);

    unsigned long t = millis();
    while (client.connected() && (millis() - t < 7000)) {
      if (client.available()) {
        String line = client.readStringUntil('\n');
        if (line.startsWith("HTTP/1.1 200 OK") || line.indexOf("\"ok\":true") != -1) {
          ok = true;
          break;
        }
      } else {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }
    client.stop();
  }

  releaseTelegramLock();
  return ok;
}

String buildReportMessage() {
  AverageStats m15 = calculateAverages(180);
  AverageStats m60 = calculateAverages(720);

  String rawIp = (isApMode) ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  String displayIp = showIpOnDisplay ? rawIp : maskIpAddress(rawIp);

  int currentAqi = calculateAQI(currentData.mq135_ppm_comp);

  String msg = "";
  msg.reserve(1500);
  msg += "╔══════════════════════════════════╗\n";
  msg += "║  🛰️ <b>ATALAIA 3 — RELATÓRIO</b>   ║\n";
  msg += "╚══════════════════════════════════╝\n";
  msg += "• <b>Estado:</b> " + String(isSystemStandby ? "🌙 STANDBY" : "☀️ OPERACIONAL") + "\n";
  msg += "• <b>Risco:</b> <code>" + String(getRiskLevelName((RiskLevel)currentData.risk_level)) + "</code>\n";
  msg += "• <b>TinyML Borda:</b> <code>" + String(AtalaiaModel::getStateName((MLState)currentData.ml_state)) + "</code>\n";
  msg += "• <b>Localização:</b> " + locationInfo + "\n";
  msg += "• <b>Coordenadas:</b> " + locationLatitude + ", " + locationLongitude + "\n";
  msg += "• <b>Data/Hora:</b> " + getFormattedDateTime() + "\n";
  msg += "• <b>Tempo Ativo:</b> " + String(millis() / 3600000UL) + "h " + String((millis() % 3600000UL) / 60000UL) + "m\n";
  msg += "• <b>Painel IP:</b> http://" + displayIp + "\n\n";

  msg += "📊 <b>SENSORES EM TEMPO REAL:</b>\n";
  msg += "🌡️ <b>Temp:</b> " + String(currentData.temperature, 1) + " °C (Sensação: " + String(currentData.heat_index, 1) + " °C)\n";
  msg += "📈 <b>Taxa dT/dt:</b> " + (currentData.temp_rate_minute >= 0 ? String("+") : String("")) + String(currentData.temp_rate_minute, 2) + " °C/min\n";
  msg += "💧 <b>Umidade:</b> " + String(currentData.humidity, 1) + " % (Orvalho: " + String(currentData.dew_point, 1) + " °C)\n";
  msg += "🧪 <b>Gás:</b> Leitura MQ ADC " + String(currentData.mq135_raw) + " (~" + String((int)currentData.mq135_ppm_comp) + " ppm)\n";
  msg += "🍃 <b>AQI:</b> " + String(currentAqi) + "/500 [" + getAQICategory(currentAqi) + "]\n";
  msg += "🔥 <b>Fogo IR (D33):</b> " + String(currentData.flame_detected ? "🚨 SIM (CHAMA DETECTADA!)" : "✅ Não (Seguro)") + " [" + (flameActiveLow ? "LOW" : "HIGH") + " Ativo]\n";
  msg += "🌧️ <b>Chuva:</b> " + String(currentData.rain_detected ? "🌧️ SIM (DETECTADA)" : "☀️ Não (Seco)") + " [ADC: " + String(currentData.rain_raw) + "]\n\n";

  msg += "💾 <b>FLASH & LOTE RAM:</b>\n";
  msg += "• Registros Flash: " + String(ringMeta.head_index) + "/" + String(RING_BUFFER_SIZE) + " (Total: " + String(ringMeta.total_saved) + ")\n";
  msg += "• Lote em RAM: " + String(getPendingBatchCount()) + "/" + String(RAM_BATCH_SIZE) + " amostras\n\n";

  msg += "📈 <b>MÉDIAS HISTÓRICAS:</b>\n";
  if (m15.count > 0) {
    msg += "• <b>15 min (N=" + String(m15.count) + "):</b> " + String(m15.avgTemp, 1) + " °C | " + String(m15.avgHum, 1) + " % | ~" + String((int)m15.avgMq) + " ppm\n";
  } else {
    msg += "• <b>15 min:</b> Amostras insuficientes...\n";
  }

  if (m60.count > 0) {
    msg += "• <b>1 hora (N=" + String(m60.count) + "):</b> " + String(m60.avgTemp, 1) + " °C (" + String(m60.minTemp, 1) + "° a " + String(m60.maxTemp, 1) + "°) | " + String(m60.avgHum, 1) + " % | ~" + String((int)m60.avgMq) + " ppm\n";
  } else {
    msg += "• <b>1 hora:</b> Amostras insuficientes...\n";
  }

  return msg;
}

String buildTrendMessage() {
  const int PTS = 16;
  float tVals[PTS];
  float hVals[PTS];
  float gVals[PTS];

  for (int i = 0; i < PTS; i++) {
    tVals[i] = currentData.temperature;
    hVals[i] = currentData.humidity;
    gVals[i] = (float)currentData.mq135_raw;
  }

  if (ringMeta.total_saved > 0) {
    File f = LittleFS.open(FILE_BUFFER, FILE_READ);
    if (f) {
      int available = (ringMeta.total_saved < RING_BUFFER_SIZE) ? ringMeta.total_saved : RING_BUFFER_SIZE;
      int toRead = min(available, PTS);
      for (int i = 0; i < toRead; i++) {
        int idx = (int)ringMeta.head_index - 1 - i;
        if (idx < 0) idx += RING_BUFFER_SIZE;

        unsigned long pos = sizeof(StorageHeader) + ((unsigned long)idx * sizeof(SensorData));
        f.seek(pos, SeekSet);
        SensorData s;
        if (f.read((uint8_t*)&s, sizeof(SensorData)) == sizeof(SensorData)) {
          int slot = PTS - 1 - i;
          if (slot >= 0) {
            tVals[slot] = s.temperature;
            hVals[slot] = s.humidity;
            gVals[slot] = (float)s.mq135_raw;
          }
        }
      }
      f.close();
    }
  }

  String msg = "";
  msg.reserve(700);
  msg += "📈 <b>MINI-GRÁFICOS DE TENDÊNCIA TEMPORAL</b>\n";
  msg += "━━━━━━━━━━━━━━━━━━━━━━━━\n";
  msg += "🌡️ <b>Temperatura:</b>\n<code>" + UnicodeGraphics::sparkline(tVals, PTS) + "</code> " + String(currentData.temperature, 1) + " °C\n\n";
  msg += "💧 <b>Umidade:</b>\n<code>" + UnicodeGraphics::sparkline(hVals, PTS) + "</code> " + String(currentData.humidity, 1) + " %\n\n";
  msg += "🧪 <b>Leitura Sensor MQ (ADC):</b>\n<code>" + UnicodeGraphics::sparkline(gVals, PTS) + "</code> ADC " + String(currentData.mq135_raw) + "\n";
  msg += "━━━━━━━━━━━━━━━━━━━━━━━━\n";
  msg += "<i>Tempo ➔.</i>";
  return msg;
}

String buildHealthMessage() {
  SystemHealth h = getSystemHealth();
  uint32_t totalHeapEstimate = h.freeHeap + 64000;
  float heapUsagePct = 100.0f - ((float)h.freeHeap / (float)totalHeapEstimate * 100.0f);

  String msg = "";
  msg.reserve(800);
  msg += "🩺 <b>TELEMETRIA E SAÚDE DO ESP32</b>\n";
  msg += "━━━━━━━━━━━━━━━━━━━━━━━━\n";
  msg += "• <b>Temp Silício (SoC):</b> <b>" + String(h.cpuTemperature, 1) + " °C</b>\n";
  msg += "• <b>Frequência Xtensa:</b> " + String(h.cpuFreqMHz) + " MHz\n";
  msg += "• <b>Causa do Reset:</b> <code>" + h.resetReason + "</code>\n";
  msg += "• <b>Tempo Ligado:</b> " + String(h.uptimeSeconds / 3600) + "h " + String((h.uptimeSeconds % 3600) / 60) + "m " + String(h.uptimeSeconds % 60) + "s\n\n";

  msg += "💾 <b>OCUPAÇÃO DE MEMÓRIA HEAP:</b>\n";
  msg += "<code>" + UnicodeGraphics::progressBar(heapUsagePct, 12) + "</code>\n";
  msg += "• Heap Livre Atual: " + String(h.freeHeap / 1024) + " kB\n";
  msg += "• Mínimo Histórico: " + String(h.minFreeHeap / 1024) + " kB\n";
  msg += "• Maior Bloco Contíguo: " + String(h.maxAllocHeap / 1024) + " kB\n";
  msg += "• Fragmentação Estimada: " + String(h.heapFragmentation) + " %\n\n";

  msg += "📶 <b>ENLACE WI-FI:</b>\n";
  msg += "• Nível RSSI: " + String(h.wifiRssi) + " dBm\n";
  msg += "• Qualidade do Enlace: <code>" + UnicodeGraphics::progressBar((float)h.wifiSignalQuality, 10) + "</code>";

  return msg;
}

String buildStatsMessage() {
  AverageStats m60 = calculateAverages(720);
  AverageStats m12h = calculateAverages(RING_BUFFER_SIZE);

  String msg = "";
  msg.reserve(700);
  msg += "📊 <b>ESTATÍSTICAS HISTÓRICAS CONSOLIDADAS</b>\n";
  msg += "━━━━━━━━━━━━━━━━━━━━━━━━\n";
  msg += "🕒 <b>Última 1 Hora (N=" + String(m60.count) + "):</b>\n";
  msg += "• Temp: Méd " + String(m60.avgTemp, 1) + " °C | Mín " + String(m60.minTemp, 1) + "° | Máx " + String(m60.maxTemp, 1) + "°\n";
  msg += "• Umid: Méd " + String(m60.avgHum, 1) + " % | Mín " + String(m60.minHum, 1) + "% | Máx " + String(m60.maxHum, 1) + "%\n";
  msg += "• Gás: Méd ~" + String((int)m60.avgMq) + " ppm\n\n";

  msg += "📅 <b>Janela Completa 12 Horas (N=" + String(m12h.count) + "):</b>\n";
  msg += "• Temp: Méd " + String(m12h.avgTemp, 1) + " °C | Mín " + String(m12h.minTemp, 1) + "° | Máx " + String(m12h.maxTemp, 1) + "°\n";
  msg += "• Umid: Méd " + String(m12h.avgHum, 1) + " % | Mín " + String(m12h.minHum, 1) + "% | Máx " + String(m12h.maxHum, 1) + "%\n";
  msg += "• Gás: Méd ~" + String((int)m12h.avgMq) + " ppm";

  return msg;
}

String buildNetworkMessage(bool showFullSensitiveData) {
  String rawIp = (isApMode) ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  String displayIp = (showFullSensitiveData || showIpOnDisplay) ? rawIp : maskIpAddress(rawIp);
  String rawMac = WiFi.macAddress();
  String displayMac = showFullSensitiveData ? rawMac : maskMacAddress(rawMac);

  String netMsg = "";
  netMsg.reserve(750);
  netMsg += showFullSensitiveData ? "📡 <b>DIAGNÓSTICO TOTAL DE REDE (IRRESTRITO)</b>\n"
                                  : "🛡️ <b>DIAGNÓSTICO SEGURO DE REDE (PROTEGIDO)</b>\n";
  netMsg += "━━━━━━━━━━━━━━━━━━━━━━━━\n";
  netMsg += "• <b>Modo Operacional:</b> " + String(isApMode ? "SoftAP Seguro (atalaia3)" : "Station Conectado") + "\n";
  netMsg += "• <b>SSID:</b> " + String(isApMode ? AP_SSID : WiFi.SSID()) + "\n";

  if (!isApMode) {
    netMsg += "• <b>Sinal Wi-Fi:</b> " + String(WiFi.RSSI()) + " dBm\n";
    String rawGw = WiFi.gatewayIP().toString();
    netMsg += "• <b>Gateway:</b> " + (showFullSensitiveData ? rawGw : maskIpAddress(rawGw)) + "\n";
  }

  netMsg += "• <b>Endereço IP:</b> " + displayIp + "\n";
  netMsg += "• <b>Exibição IP no OLED:</b> " + String(showIpOnDisplay ? "ATIVADA" : "OCULTA") + "\n";
  netMsg += "• <b>Painel Direto:</b> http://" + displayIp + "\n";
  netMsg += "• <b>Acesso mDNS:</b> http://atalaia3.local\n";
  netMsg += "• <b>MAC Address:</b> " + displayMac + "\n";
  netMsg += "• <b>Memória Heap:</b> " + String(ESP.getFreeHeap() / 1024) + " kB\n";
  netMsg += "• <b>Lote RAM Pendente:</b> " + String(getPendingBatchCount()) + "/" + String(RAM_BATCH_SIZE) + "\n";

  return netMsg;
}

String buildLogsMessage() {
  String msg = "";
  msg.reserve(1000);
  msg += "📋 <b>ÚLTIMOS REGISTOS DO SISTEMA:</b>\n";
  msg += "━━━━━━━━━━━━━━━━━━━━━━━━\n";
  if (eventLogCount == 0) {
    msg += "Nenhum evento registrado até ao momento.\n";
    return msg;
  }
  int countToShow = min(eventLogCount, 12);
  for (int i = 0; i < countToShow; i++) {
    int idx = (eventLogHead - 1 - i + MAX_EVENT_LOGS) % MAX_EVENT_LOGS;
    msg += "• [" + eventLogs[idx].timeStr + "] <b>" + eventLogs[idx].category + ":</b> " + eventLogs[idx].message + "\n";
  }
  return msg;
}

struct ParsedTelegramCommand {
  int64_t senderId;
  String text;
};

void pollTelegramCommands() {
  if (WiFi.status() != WL_CONNECTED || WiFi.localIP()[0] == 0) return;

  if (!botInitialized) {
    if (!initTelegramBot()) return;
  }

  if (!checkTlsHeapAvailability()) return;
  if (!acquireTelegramLock(pdMS_TO_TICKS(1500))) return;

  String resp = "";
  resp.reserve(3072);

  {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(4000);

    if (!client.connect("api.telegram.org", 443)) {
      releaseTelegramLock();
      return;
    }

    String url = "/bot" + String(TELEGRAM_BOT_TOKEN) + "/getUpdates?limit=5&timeout=0";
    if (lastTelegramUpdateId > 0) {
      url += "&offset=" + String((long long)(lastTelegramUpdateId + 1));
    }

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: api.telegram.org\r\n" +
                 "User-Agent: ESP32-ATALAIA3\r\n" +
                 "Connection: close\r\n\r\n");

    unsigned long startWait = millis();
    while (!client.available() && client.connected() && (millis() - startWait < 2500)) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    bool headerDone = false;
    unsigned long readStart = millis();

    while ((client.connected() || client.available()) && (millis() - readStart < 3500)) {
      if (client.available()) {
        if (!headerDone) {
          String hLine = client.readStringUntil('\n');
          hLine.trim();
          if (hLine.length() == 0) headerDone = true;
        } else {
          char c = (char)client.read();
          resp += c;
          if (resp.length() > 3000) break;
        }
      } else {
        vTaskDelay(pdMS_TO_TICKS(5));
      }
    }
    client.stop();
  }

  releaseTelegramLock();

  if (resp.indexOf("\"ok\":true") == -1) return;
  if (resp.indexOf("\"update_id\":") == -1) return;

  const int MAX_CMDS_IN_BATCH = 5;
  ParsedTelegramCommand commandQueue[MAX_CMDS_IN_BATCH];
  int commandCount = 0;

  int uIdx = resp.indexOf("\"update_id\":");
  while (uIdx != -1 && commandCount < MAX_CMDS_IN_BATCH) {
    int nextUIdx = resp.indexOf("\"update_id\":", uIdx + 12);
    String updateBlock = (nextUIdx != -1) ? resp.substring(uIdx, nextUIdx) : resp.substring(uIdx);

    int uEnd = updateBlock.indexOf(",");
    if (uEnd != -1) {
      String uidStr = updateBlock.substring(12, uEnd);
      uidStr.trim();
      int64_t currentUid = strtoll(uidStr.c_str(), NULL, 10);
      if (currentUid >= lastTelegramUpdateId) {
        lastTelegramUpdateId = currentUid;
      }
    }

    int64_t senderId = 0;
    int cIdx = updateBlock.indexOf("\"chat\":");
    if (cIdx != -1) {
      int idIdx = updateBlock.indexOf("\"id\":", cIdx);
      if (idIdx != -1) {
        int idEnd = updateBlock.indexOf(",", idIdx + 5);
        if (idEnd == -1) idEnd = updateBlock.indexOf("}", idIdx + 5);
        if (idEnd != -1) {
          String cidStr = updateBlock.substring(idIdx + 5, idEnd);
          cidStr.trim();
          senderId = strtoll(cidStr.c_str(), NULL, 10);
        }
      }
    }

    if (validateSenderAuthorization(senderId)) {
      int tIdx = updateBlock.indexOf("\"text\":");
      if (tIdx != -1) {
        int valStart = updateBlock.indexOf("\"", tIdx + 7);
        if (valStart != -1) {
          int valEnd = updateBlock.indexOf("\"", valStart + 1);
          while (valEnd != -1 && updateBlock.charAt(valEnd - 1) == '\\') {
            valEnd = updateBlock.indexOf("\"", valEnd + 1);
          }
          if (valEnd != -1) {
            String cmdText = updateBlock.substring(valStart + 1, valEnd);
            cmdText.trim();
            commandQueue[commandCount].senderId = senderId;
            commandQueue[commandCount].text = cmdText;
            commandCount++;
          }
        }
      }
    }

    uIdx = nextUIdx;
  }

  for (int i = 0; i < commandCount; i++) {
    String cmd = commandQueue[i].text;
    String cmdLower = cmd;
    cmdLower.toLowerCase();

    if (cmdLower.indexOf("@") != -1) {
      int atIdx = cmdLower.indexOf("@");
      int spaceAfter = cmdLower.indexOf(" ", atIdx);
      if (spaceAfter != -1) {
        cmdLower = cmdLower.substring(0, atIdx) + cmdLower.substring(spaceAfter);
      } else {
        cmdLower = cmdLower.substring(0, atIdx);
      }
    }

    Serial.printf("[TELEGRAM] Executando comando: '%s'\n", cmd.c_str());

    // 1. Navegação de Telas e Display
    if (cmdLower == "/proxima" || cmdLower == "/next") {
      nextDisplayScreen();
      notifyOledTelegramTask("Tela Avancada", String(getDisplayScreenName(getCurrentDisplayScreen())));
      sendTelegramMessage("🖥️ <b>Display OLED:</b> Próxima tela selecionada ➔ <code>" + String(getDisplayScreenName(getCurrentDisplayScreen())) + "</code> [" + String(getCurrentDisplayScreen() + 1) + "/" + String(getTotalDisplayScreens()) + "]");
    } else if (cmdLower == "/anterior" || cmdLower == "/prev") {
      previousDisplayScreen();
      notifyOledTelegramTask("Tela Anterior", String(getDisplayScreenName(getCurrentDisplayScreen())));
      sendTelegramMessage("🖥️ <b>Display OLED:</b> Tela anterior selecionada ➔ <code>" + String(getDisplayScreenName(getCurrentDisplayScreen())) + "</code> [" + String(getCurrentDisplayScreen() + 1) + "/" + String(getTotalDisplayScreens()) + "]");
    } else if (cmdLower.startsWith("/tela ") || cmdLower.startsWith("tela ")) {
      int spaceIdx = cmd.indexOf(' ');
      int scNum = cmd.substring(spaceIdx + 1).toInt() - 1;
      if (scNum >= 0 && scNum < getTotalDisplayScreens()) {
        selectDisplayScreen((uint8_t)scNum);
        notifyOledTelegramTask("Tela Direta", String(getDisplayScreenName(scNum)));
        sendTelegramMessage("🖥️ <b>Display OLED:</b> Comutado para tela " + String(scNum + 1) + ": <code>" + String(getDisplayScreenName(scNum)) + "</code>");
      } else {
        sendTelegramMessage("❌ <b>Índice inválido!</b> Telas disponíveis de 1 a " + String(getTotalDisplayScreens()) + ".");
      }
    } else if (cmdLower == "/telas" || cmdLower == "/indice") {
      String tList = "🖥️ <b>ÍNDICE DE TELAS DO ATALAIA 3:</b>\n━━━━━━━━━━━━━━━━━━━━━━━━\n";
      for (uint8_t sc = 0; sc < getTotalDisplayScreens(); sc++) {
        tList += String(sc + 1) + ". <b>" + String(getDisplayScreenName(sc)) + "</b>" + ((sc == getCurrentDisplayScreen()) ? " 📍 [ATIVA]" : "") + "\n";
      }
      tList += "\n<i>Use</i> <code>/tela &lt;num&gt;</code> <i>para alternar diretamente.</i>";
      sendTelegramMessage(tList);
    } else if (cmdLower == "/fixar" || cmdLower == "/pin") {
      setDisplayPinned(true);
      notifyOledTelegramTask("Display", "TELA FIXADA");
      sendTelegramMessage("📌 <b>Tela Celular/Terminal FIXADA no OLED!</b>\nO carrossel automático foi pausado.");
    } else if (cmdLower == "/soltar" || cmdLower == "/unpin" || cmdLower == "/liberar") {
      setDisplayPinned(false);
      notifyOledTelegramTask("Display", "CARROSSEL ATIVO");
      sendTelegramMessage("🔄 <b>Tela OLED liberada!</b>\nO carrossel voltou a alternar as páginas normalmente.");
    } else if (cmdLower == "/ip_display on" || cmdLower == "/ip_display 1" || cmdLower == "/mostrar_ip") {
      showIpOnDisplay = true;
      saveAudioAndPowerConfig();
      sendTelegramMessage("👁️ <b>Exibição do IP no Display OLED ATIVADA!</b>");
    } else if (cmdLower == "/ip_display off" || cmdLower == "/ip_display 0" || cmdLower == "/ocultar_ip") {
      showIpOnDisplay = false;
      saveAudioAndPowerConfig();
      sendTelegramMessage("🙈 <b>Exibição do IP no Display OLED OCULTADA!</b> (Modo Protegido)");
    }
    // 2. Imagens Modulares e Efeitos
    else if (cmdLower == "/imagens") {
      String imgMsg = "🖼️ <b>IMAGENS REGISTRADAS NO FIRMWARE:</b>\n━━━━━━━━━━━━━━━━━━━━━━━━\n";
      for (uint8_t idx = 0; idx < SYSTEM_IMAGE_COUNT; idx++) {
        imgMsg += String(idx + 1) + ". <b>" + String(SYSTEM_IMAGES[idx].name) + "</b> (" + String(SYSTEM_IMAGES[idx].width) + "x" + String(SYSTEM_IMAGES[idx].height) + ")\n";
      }
      imgMsg += "\n<i>Use</i> <code>/imagem &lt;num&gt; [tempo_seg]</code> <i>para projetar no OLED.</i>";
      sendTelegramMessage(imgMsg);
    } else if (cmdLower.startsWith("/imagem ") || cmdLower.startsWith("imagem ")) {
      int space1 = cmd.indexOf(' ');
      int space2 = cmd.indexOf(' ', space1 + 1);
      int imgIdx = 0;
      unsigned long durSec = 5;
      if (space2 != -1) {
        imgIdx = cmd.substring(space1 + 1, space2).toInt() - 1;
        durSec = cmd.substring(space2 + 1).toInt();
      } else {
        imgIdx = cmd.substring(space1 + 1).toInt() - 1;
      }
      if (imgIdx >= 0 && imgIdx < SYSTEM_IMAGE_COUNT) {
        if (durSec < 1) durSec = 5;
        triggerDisplayCustomImage((uint8_t)imgIdx, durSec * 1000UL);
        sendTelegramMessage("🖼️ <b>Imagem projetada no OLED!</b>\n• Imagem: <b>" + String(SYSTEM_IMAGES[imgIdx].name) + "</b>\n• Tempo: <b>" + String(durSec) + " segundos</b> sem travar a estação.");
      } else {
        sendTelegramMessage("❌ <b>Índice de imagem inválido!</b> Digite <code>/imagens</code> para ver a lista.");
      }
    } else if (cmdLower == "/parar_imagem") {
      stopDisplayCustomImage();
      sendTelegramMessage("⏹️ Projeção de imagem interrompida no display.");
    } else if (cmdLower == "/natal" || cmdLower == "/test_rain" || cmdLower == "natal" || cmdLower == "rain") {
      notifyOledTelegramTask("Melodia Natal", "Jingle Bells Flash");
      sendTelegramMessage("🎄 <b>Executando partitura natalícia e animação no OLED...</b>");
      startRainMelody(true);
    } else if (cmdLower.startsWith("/fogo_polaridade")) {
      flameActiveLow = !flameActiveLow;
      saveAudioAndPowerConfig();
      sendTelegramMessage("🔥 <b>Polaridade do Sensor de Fogo (D33) alterada!</b>\nNovo modo: <b>" + String(flameActiveLow ? "Nível LOW Ativo (Padrão LM393)" : "Nível HIGH Ativo") + "</b>");
    }
    // 3. Mensagens diretas e terminal hacker
    else if (!cmd.startsWith("/")) {
      pushTelegramMessageToDisplay("TG", cmd);
      sendTelegramMessage("📱 <b>Mensagem projetada no display OLED!</b>\n<code>[TG]: " + cmd + "</code>");
    } else if (cmdLower.startsWith("/msg ") || cmdLower.startsWith("/escrever ")) {
      int spaceIdx = cmd.indexOf(' ');
      String userMsg = cmd.substring(spaceIdx + 1);
      userMsg.trim();
      pushTelegramMessageToDisplay("TG", userMsg);
      sendTelegramMessage("📱 <b>Mensagem projetada no display OLED!</b>\n<code>" + userMsg + "</code>");
    } else if (cmdLower.startsWith("/cmd ") || cmdLower.startsWith("/terminal ") || cmdLower.startsWith("/hacker ")) {
      int spaceIdx = cmd.indexOf(' ');
      String hackMsg = cmd.substring(spaceIdx + 1);
      hackMsg.trim();
      triggerDisplayHackerTerminal(hackMsg);
      sendTelegramMessage("💻 <b>Comando enviado para o Terminal OLED!</b>\n<code>root@atalaia3:~# " + hackMsg + "</code>");
    }
    // 4. Telemetria e Relatórios
    else if (cmdLower == "/report" || cmdLower == "/status" || cmdLower == "report" || cmdLower == "status") {
      notifyOledTelegramTask("Relatorio", "Gerando telemetria...");
      sendTelegramMessage(buildReportMessage());
    } else if (cmdLower == "/trend" || cmdLower == "/grafico" || cmdLower == "trend") {
      notifyOledTelegramTask("Graficos", "Plotando Unicode...");
      sendTelegramMessage(buildTrendMessage());
    } else if (cmdLower == "/health" || cmdLower == "/saude" || cmdLower == "health") {
      notifyOledTelegramTask("Saude SoC", "Telemetria ESP32...");
      sendTelegramMessage(buildHealthMessage());
    } else if (cmdLower == "/stats" || cmdLower == "/medias" || cmdLower == "stats") {
      notifyOledTelegramTask("Estatisticas", "Calculando 1h/12h...");
      sendTelegramMessage(buildStatsMessage());
    } else if (cmdLower == "/events" || cmdLower == "/logs" || cmdLower == "/log" || cmdLower == "events") {
      notifyOledTelegramTask("Eventos", "Obtendo logs...");
      sendTelegramMessage(buildLogsMessage());
    }
    // 5. Banco de Dados LittleFS e CSV
    else if (cmdLower == "/export" || cmdLower == "/csv" || cmdLower == "export") {
      notifyOledTelegramTask("Exportacao CSV", "Streaming Flash...");
      sendTelegramMessage("📊 A compilar telemetria em formato CSV com coordenadas...");
      sendTelegramCsvData("📊 Exportação CSV de Telemetria — ATALAIA 3");
    } else if (cmdLower == "/backup" || cmdLower == "/db" || cmdLower == "/download" || cmdLower == "backup") {
      notifyOledTelegramTask("Backup Flash", "Dump binario ATA3...");
      sendTelegramMessage("📤 Preparando envio do banco de dados LittleFS...");
      sendTelegramDocument(FILE_BUFFER, "📦 Banco de Dados Binário ATALAIA 3\n• Registros: " + String(ringMeta.head_index) + "/" + String(RING_BUFFER_SIZE));
    } else if (cmdLower == "/reset_db" || cmdLower == "/limpar_db" || cmdLower == "/zerar_db") {
      notifyOledTelegramTask("Zerar Flash", "Limpando 12h buffer");
      resetBufferStorage();
      sendTelegramMessage("🗑️ <b>Banco de Dados Reinicializado!</b>\nBuffer de 12 horas zerado.");
    } else if (cmdLower == "/purge" || cmdLower == "/limpar_telegram" || cmdLower == "/limpar_fila") {
      notifyOledTelegramTask("Purga Fila", "Limpando Telegram API");
      bool purged = purgeTelegramPendingUpdates();
      sendTelegramMessage(purged ? "🧹 <b>Fila do Telegram limpa com sucesso!</b>" : "❌ Falha ao limpar fila do Telegram.");
    }
    // 6. Rede e Energia
    else if (cmdLower == "/network" || cmdLower == "/ip" || cmdLower == "network" || cmdLower == "ip") {
      notifyOledTelegramTask("Info Rede", "Modo Protegido");
      sendTelegramMessage(buildNetworkMessage(false));
    } else if (cmdLower == "/network_total" || cmdLower == "/ip_total" || cmdLower == "network_total" || cmdLower == "ip_total") {
      notifyOledTelegramTask("Info Rede Total", "Modo Irrestrito");
      sendTelegramMessage(buildNetworkMessage(true));
    } else if (cmdLower == "/on" || cmdLower == "/ligar" || cmdLower == "/wake" || cmdLower == "on") {
      notifyOledTelegramTask("Operacional", "Estacao Ativada");
      setSystemStandby(false, true);
    } else if (cmdLower == "/off" || cmdLower == "/desligar" || cmdLower == "/standby" || cmdLower == "off") {
      notifyOledTelegramTask("Standby", "Estacao Pausada");
      setSystemStandby(true, true);
    } else if (cmdLower == "/reboot" || cmdLower == "/reiniciar" || cmdLower == "reboot") {
      notifyOledTelegramTask("Reboot CPU", "Salvando e reiniciando");
      flushRamBatch();
      saveMetadataNow();
      sendTelegramMessage("🔄 Reiniciando ESP32...");
      delay(300);
      ESP.restart();
    } else if (cmdLower == "/ap" || cmdLower == "ap") {
      notifyOledTelegramTask("Modo SoftAP", "Iniciando atalaia3");
      sendTelegramMessage("📡 Comutando para Ponto de Acesso Seguro (atalaia3 em 192.168.4.1)...");
      delay(100);
      startAccessPoint();
    } else if (cmdLower == "/sta" || cmdLower == "sta") {
      notifyOledTelegramTask("Modo Station", "Conectando ao roteador");
      sendTelegramMessage("🔄 Retomando modo Station com credenciais salvas...");
      delay(100);
      setupNetworkStack();
    }
    // 7. Áudio e Testes de Sirene
    else if (cmdLower == "/sound_on" || cmdLower == "/som_on") {
      masterSoundEnabled = true;
      saveAudioAndPowerConfig();
      notifyOledTelegramTask("Som Mestre", "HABILITADO");
      sendTelegramMessage("🔊 <b>Som Mestre LIGADO!</b>");
    } else if (cmdLower == "/sound_off" || cmdLower == "/som_off") {
      masterSoundEnabled = false;
      saveAudioAndPowerConfig();
      notifyOledTelegramTask("Som Mestre", "MUTADO");
      sendTelegramMessage("🔇 <b>Som Mestre MUTADO!</b>");
    } else if (cmdLower == "/beep_on") {
      collectionBeepEnabled = true;
      saveAudioAndPowerConfig();
      notifyOledTelegramTask("Bip Coleta", "ATIVADO (12ms)");
      sendTelegramMessage("🔔 <b>Bip de Coleta: ATIVADO</b>");
    } else if (cmdLower == "/beep_off") {
      collectionBeepEnabled = false;
      saveAudioAndPowerConfig();
      notifyOledTelegramTask("Bip Coleta", "DESATIVADO");
      sendTelegramMessage("🔕 <b>Bip de Coleta: DESATIVADO</b>");
    } else if (cmdLower == "/beep_dev_5v") {
      selectedBeepDevice = BEEP_DEV_BUZZER_5V;
      saveAudioAndPowerConfig();
      notifyOledTelegramTask("Atuador Bip", "Buzzer 5V (D27)");
      sendTelegramMessage("🔊 Dispositivo de Coleta: <b>Buzzer 5V (GPIO 27)</b>");
    } else if (cmdLower == "/beep_dev_3v") {
      selectedBeepDevice = BEEP_DEV_BUZZER_3V;
      saveAudioAndPowerConfig();
      notifyOledTelegramTask("Atuador Bip", "Buzzer 3.3V (D18)");
      sendTelegramMessage("🔊 Dispositivo de Coleta: <b>Buzzer 3.3V (GPIO 18)</b>");
    } else if (cmdLower == "/beep_dev_all") {
      selectedBeepDevice = BEEP_DEV_ALL;
      saveAudioAndPowerConfig();
      notifyOledTelegramTask("Atuador Bip", "AMBOS (D18 + D27)");
      sendTelegramMessage("🔊 Dispositivo de Coleta: <b>AMBOS OS BUZZERS (D18 + D27)</b>");
    } else if (cmdLower.startsWith("/gas ") || cmdLower.startsWith("gas ")) {
      int spaceIdx = cmd.indexOf(' ');
      String valStr = cmd.substring(spaceIdx + 1);
      valStr.trim();
      float val = valStr.toFloat();
      if (val >= GAS_PPM_MIN_ALLOWED && val <= GAS_PPM_MAX_ALLOWED) {
        gasAlertThresholdPpm = val;
        saveAudioAndPowerConfig();
        notifyOledTelegramTask("Limiar Gas", String((int)val) + " ppm");
        sendTelegramMessage("✅ <b>Limiar de Alarme de Gás:</b> " + String((int)gasAlertThresholdPpm) + " ppm.");
      } else {
        sendTelegramMessage("❌ <b>Valor fora da faixa!</b> Informe entre <b>50</b> e <b>5000</b> ppm.");
      }
    } else if (cmdLower == "/siren_on") {
      riskSirensEnabled = true;
      saveAudioAndPowerConfig();
      notifyOledTelegramTask("Sirenes Risco", "ARMADAS");
      sendTelegramMessage("🚨 <b>Sirenes de Risco: ARMADAS</b>");
    } else if (cmdLower == "/siren_off") {
      riskSirensEnabled = false;
      stopSiren();
      stopGasAlarm();
      stopRainMelody();
      saveAudioAndPowerConfig();
      notifyOledTelegramTask("Sirenes Risco", "DESARMADAS");
      sendTelegramMessage("⚠️ <b>Sirenes de Risco: DESARMADAS</b>");
    } else if (cmdLower == "/test_beep" || cmdLower == "/beep") {
      notifyOledTelegramTask("Teste Bip", "Tick Atomico 12ms");
      triggerManualBeepTest(selectedBeepDevice, 40);
      sendTelegramMessage("✅ Bip rápido executado.");
    } else if (cmdLower == "/test_5v" || cmdLower == "/beep5v") {
      notifyOledTelegramTask("Teste Bip 5V", "Pino GPIO 27");
      triggerManualBeepTest(BEEP_DEV_BUZZER_5V, 45);
      sendTelegramMessage("✅ Teste curto Buzzer 5V (D27) executado.");
    } else if (cmdLower == "/test_3v" || cmdLower == "/beep3v") {
      notifyOledTelegramTask("Teste Bip 3.3V", "PWM GPIO 18");
      triggerManualBeepTest(BEEP_DEV_BUZZER_3V, 40);
      sendTelegramMessage("✅ Teste curto Buzzer 3.3V (D18) executado.");
    } else if (cmdLower == "/test_all" || cmdLower == "/beepall") {
      notifyOledTelegramTask("Teste Duplo", "D18 + D27");
      triggerManualBeepTest(BEEP_DEV_ALL, 50);
      sendTelegramMessage("✅ Teste duplo rápido acionado!");
    } else if (cmdLower == "/test_siren" || cmdLower == "/siren_test" || cmdLower == "/siren") {
      notifyOledTelegramTask("Sirene Incendio", "Varredura 3.5s D18+D27");
      startSiren(true, 3500);
      sendTelegramMessage("🚨 <b>Sirene de Incêndio Louca acionada (3.5s)!</b>\nBuzzer 3.3V (D18) em varredura e Buzzer 5V (D27) em estrobo.");
    } else if (cmdLower == "/test_gas" || cmdLower == "/gas_test" || cmdLower == "/gas_alarm") {
      notifyOledTelegramTask("Alarme Gas", "Pulsos 3.1kHz 3.5s");
      startGasAlarm(true, 3500);
      sendTelegramMessage("⚠️ <b>Alarme Sonoro de Gás acionado (3.5s)!</b>");
    } else if (cmdLower == "/stop_sound" || cmdLower == "/stop" || cmdLower == "stop") {
      notifyOledTelegramTask("Parar Sons", "Atuadores Silenciados");
      stopAllSounds();
      sendTelegramMessage("🛑 Todos os atuadores sonoros foram desligados.");
    }
    // 8. Controle do LED Verde
    else if (cmdLower == "/led auto") {
      setLedMode(LED_MODE_AUTO_HEARTBEAT);
      saveAudioAndPowerConfig();
      notifyOledTelegramTask("LED Status", "Modo Heartbeat Auto");
      sendTelegramMessage("💚 <b>LED Verde: Modo Automático Heartbeat.</b>");
    } else if (cmdLower == "/led on") {
      setLedMode(LED_MODE_FORCE_ON);
      saveAudioAndPowerConfig();
      notifyOledTelegramTask("LED Status", "FORCADO LIGADO");
      sendTelegramMessage("💡 <b>LED Verde: FORÇADO ON.</b>");
    } else if (cmdLower == "/led off") {
      setLedMode(LED_MODE_FORCE_OFF);
      saveAudioAndPowerConfig();
      notifyOledTelegramTask("LED Status", "FORCADO DESLIGADO");
      sendTelegramMessage("🌑 <b>LED Verde: FORÇADO OFF.</b>");
    }
    // 9. Menu de Ajuda Completo
    else if (cmdLower == "/help" || cmdLower == "/start" || cmdLower == "/ajuda") {
      notifyOledTelegramTask("Menu Ajuda", "Enviando comandos...");
      String h = "";
      h.reserve(1500);
      h += "╔════════════════════════════════════╗\n";
      h += "║ 🤖 <b>CENTRAL DE COMANDOS — ATALAIA 3</b>   ║\n";
      h += "╚════════════════════════════════════╝\n";
      h += "🖥️ <b>Navegação e Telas do Display OLED:</b>\n";
      h += "• /proxima | /anterior — Alternar páginas do carrossel\n";
      h += "• /telas ou /indice — Listar páginas disponíveis\n";
      h += "• /tela &lt;1..4&gt; — Ir diretamente para a tela desejada\n";
      h += "• /fixar | /soltar — Fixar ou liberar rotação de telas\n";
      h += "• /mostrar_ip | /ocultar_ip — Alternar exibição do IP no display\n";
      h += "• /imagens — Listar imagens gravadas na Flash\n";
      h += "• /imagem &lt;num&gt; [tempo] — Projetar imagem sem travar a CPU\n";
      h += "• /parar_imagem — Interromper imagem ativa\n\n";
      h += "📊 <b>Telemetria e Diagnósticos:</b>\n";
      h += "• /report — Relatório tático completo com coordenadas\n";
      h += "• /trend — Mini-gráficos sparkline da leitura do MQ\n";
      h += "• /health — Diagnóstico de CPU, Heap e SoC\n";
      h += "• /stats — Médias consolidadas (1h/12h)\n";
      h += "• /events — Histórico recente de eventos\n";
      h += "• /export — Descarregar CSV com Latitude e Longitude\n";
      h += "• /backup — Descarregar buffer binário LittleFS\n";
      h += "• /reset_db — Reinicializar banco de 12 horas\n";
      h += "• /network | /network_total — Diagnóstico de rede\n\n";
      h += "🔊 <b>Áudio, Alarmes e Sensores:</b>\n";
      h += "• /fogo_polaridade — Alternar LOW/HIGH ativo no sensor óptico\n";
      h += "• /sound_on | /sound_off — Chave Som Mestre\n";
      h += "• /beep_on | /beep_off — Bip de Coleta\n";
      h += "• /gas &lt;ppm&gt; — Ajustar limiar de alarme de gás\n";
      h += "• /test_siren — Testar sirene de incêndio louca (3.5s)\n";
      h += "• /test_gas — Testar alarme de gás (3.5s)\n";
      h += "• /natal — Partitura de Natal + animação OLED\n";
      h += "• /stop — Silenciar todos os atuadores\n\n";
      h += "⚡ <b>Energia & Modos de Rede:</b>\n";
      h += "• /off — Pausar em Standby\n";
      h += "• /on — Retomar modo Operacional\n";
      h += "• /reboot — Reinicializar ESP32\n";
      h += "• /ap | /sta — Alternar entre SoftAP e Station";
      sendTelegramMessage(h);
    }
  }
}