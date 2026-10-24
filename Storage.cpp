#include "Storage.h"
#include "Actuators.h"
#include "Sensors.h"
#include "TelegramBot.h"
#include "AtalaiaModel.h"
#include "DisplayManager.h"
#include "NetworkManager.h"
#include <esp_sleep.h>
#include <time.h>

String locationCityRegion = "sem local";
String locationLatitude   = "sem local";
String locationLongitude  = "sem local";
bool flameActiveLow       = true;

// Buffer em RAM para gravação em lote (wear leveling de flash)
static SensorData ramBatch[RAM_BATCH_SIZE];
static int ramBatchCount = 0;
static unsigned long lastMetaSyncTime = 0;

static uint32_t calculateCrc32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xFFFFFFFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 1) {
        crc = (crc >> 1) ^ 0xEDB88320;
      } else {
        crc >>= 1;
      }
    }
  }
  return ~crc;
}

void initStorageSystem() {
  if (!LittleFS.begin(true)) {
    Serial.println("[STORAGE] ERRO CRÍTICO: Não foi possível montar o LittleFS!");
    return;
  }

  if (!LittleFS.exists(FILE_BUFFER)) {
    File f = LittleFS.open(FILE_BUFFER, FILE_WRITE);
    if (f) {
      StorageHeader header;
      header.magic = STORAGE_MAGIC_HEADER;
      header.version = STORAGE_FORMAT_VERSION;
      header.record_size = sizeof(SensorData);
      header.capacity = RING_BUFFER_SIZE;
      header.reserved = 0;
      f.write((const uint8_t*)&header, sizeof(StorageHeader));
      f.flush();
      f.close();
      Serial.println("[STORAGE] Arquivo de buffer binário criado com cabeçalho ATA3.");
    }
  }
}

String getFormattedDateTime() {
  struct tm timeinfo;
  if (ntpSynced && getLocalTime(&timeinfo, 10)) {
    char buf[32];
    strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &timeinfo);
    return String(buf);
  }
  unsigned long sec = millis() / 1000UL;
  unsigned long h = sec / 3600UL;
  unsigned long m = (sec % 3600UL) / 60UL;
  unsigned long s = sec % 60UL;
  char buf[32];
  snprintf(buf, sizeof(buf), "Uptime %02lu:%02lu:%02lu", h, m, s);
  return String(buf);
}

void addEventLog(const String& category, const String& message, uint8_t severity) {
  eventLogs[eventLogHead].uptimeSec = millis() / 1000UL;
  eventLogs[eventLogHead].timeStr   = getFormattedDateTime();
  eventLogs[eventLogHead].category  = category;
  eventLogs[eventLogHead].message   = message;
  eventLogs[eventLogHead].severity  = severity;

  int loggedIdx = eventLogHead;
  eventLogHead = (eventLogHead + 1) % MAX_EVENT_LOGS;
  if (eventLogCount < MAX_EVENT_LOGS) {
    eventLogCount++;
  }
  Serial.printf("[%s] [%s] (SEV:%u) %s\n",
                eventLogs[loggedIdx].timeStr.c_str(),
                category.c_str(),
                severity,
                message.c_str());
}

void syncNtpTime() {
  if (WiFi.status() != WL_CONNECTED || WiFi.localIP()[0] == 0) return;
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 1200)) {
    ntpSynced = true;
    addEventLog("RELOGIO", "Data e hora sincronizadas via NTP oficial.", 0);
  }
}

void fetchGeoLocation() {
  if (WiFi.status() != WL_CONNECTED || WiFi.localIP()[0] == 0) {
    locationInfo = "(SEM LOCAL)";
    locationCityRegion = "sem local";
    locationLatitude   = "sem local";
    locationLongitude  = "sem local";
    return;
  }

  WiFiClient client;
  client.setTimeout(2500);

  if (!client.connect("ip-api.com", 80)) return;

  client.print(String("GET /json/?fields=status,city,region,lat,lon HTTP/1.1\r\n") +
               "Host: ip-api.com\r\n" +
               "User-Agent: ESP32-ATALAIA3-EDGE\r\n" +
               "Connection: close\r\n\r\n");

  unsigned long start = millis();
  String body = "";
  bool isBody = false;

  while (client.connected() && millis() - start < 3000) {
    while (client.available()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        isBody = true;
        continue;
      }
      if (isBody) body += line;
    }
  }
  client.stop();

  if (body.indexOf("\"status\":\"success\"") != -1) {
    String city = "", region = "", lat = "", lon = "";
    int cIdx = body.indexOf("\"city\":\"");
    if (cIdx != -1) city = body.substring(cIdx + 8, body.indexOf("\"", cIdx + 8));
    int rIdx = body.indexOf("\"region\":\"");
    if (rIdx != -1) region = body.substring(rIdx + 10, body.indexOf("\"", rIdx + 10));
    int latIdx = body.indexOf("\"lat\":");
    if (latIdx != -1) lat = body.substring(latIdx + 6, body.indexOf(",", latIdx));
    int lonIdx = body.indexOf("\"lon\":");
    if (lonIdx != -1) lon = body.substring(lonIdx + 6, body.indexOf("}", lonIdx));

    lat.trim();
    lon.trim();

    if (city.length() > 0) {
      locationCityRegion = city + "-" + region;
      locationLatitude   = lat;
      locationLongitude  = lon;
      locationInfo       = locationCityRegion + " (" + lat + ", " + lon + ")";
      addEventLog("GEO", "Geolocalizacao obtida: " + locationInfo, 0);
      return;
    }
  }

  locationInfo       = "(SEM LOCAL)";
  locationCityRegion = "sem local";
  locationLatitude   = "sem local";
  locationLongitude  = "sem local";
}

void loadAudioAndPowerConfig() {
  if (LittleFS.exists(FILE_AUDIO)) {
    File f = LittleFS.open(FILE_AUDIO, FILE_READ);
    if (f) {
      String s = f.readString();
      f.close();
      masterSoundEnabled     = (s.indexOf("\"master_enabled\":false") == -1);
      collectionBeepEnabled  = (s.indexOf("\"collection_beep\":false") == -1);
      riskSirensEnabled      = (s.indexOf("\"risk_sirens\":false") == -1);
      collectionFlashEnabled = (s.indexOf("\"collection_flash\":false") == -1);
      isSystemStandby        = (s.indexOf("\"standby_mode\":true") != -1);
      showIpOnDisplay        = (s.indexOf("\"show_ip\":false") == -1);
      flameActiveLow         = (s.indexOf("\"flame_active_low\":false") == -1);

      int dIdx = s.indexOf("\"beep_dev\":");
      if (dIdx != -1) {
        int devVal = s.substring(dIdx + 11, dIdx + 12).toInt();
        selectedBeepDevice = (BeepDevice)constrain(devVal, 0, 2);
      }

      int gIdx = s.indexOf("\"gas_threshold\":");
      if (gIdx != -1) {
        int gEnd = s.indexOf(",", gIdx);
        if (gEnd == -1) gEnd = s.indexOf("}", gIdx);
        float loadedPpm = s.substring(gIdx + 16, gEnd).toFloat();
        if (loadedPpm >= GAS_PPM_MIN_ALLOWED && loadedPpm <= GAS_PPM_MAX_ALLOWED) {
          gasAlertThresholdPpm = loadedPpm;
        }
      }

      if (s.indexOf("\"led_mode\":1") != -1) currentLedMode = LED_MODE_FORCE_ON;
      else if (s.indexOf("\"led_mode\":2") != -1) currentLedMode = LED_MODE_FORCE_OFF;
      else currentLedMode = LED_MODE_AUTO_HEARTBEAT;
    }
  }
}

void saveAudioAndPowerConfig() {
  File f = LittleFS.open(FILE_AUDIO, FILE_WRITE);
  if (f) {
    f.printf("{\"master_enabled\":%s,\"collection_beep\":%s,\"risk_sirens\":%s,\"collection_flash\":%s,\"standby_mode\":%s,\"show_ip\":%s,\"flame_active_low\":%s,\"beep_dev\":%d,\"gas_threshold\":%.1f,\"led_mode\":%d}",
             masterSoundEnabled ? "true" : "false",
             collectionBeepEnabled ? "true" : "false",
             riskSirensEnabled ? "true" : "false",
             collectionFlashEnabled ? "true" : "false",
             isSystemStandby ? "true" : "false",
             showIpOnDisplay ? "true" : "false",
             flameActiveLow ? "true" : "false",
             (int)selectedBeepDevice,
             gasAlertThresholdPpm,
             (int)currentLedMode);
    f.flush();
    f.close();
  }
  if (!masterSoundEnabled) {
    pwmStop3V();
    digitalTone5V(false);
  }
}

void loadWifiCredentials() {
  if (LittleFS.exists(FILE_WIFI)) {
    File f = LittleFS.open(FILE_WIFI, FILE_READ);
    if (f) {
      String content = f.readString();
      f.close();
      int sIdx = content.indexOf("\"ssid\":\"");
      int pIdx = content.indexOf("\"pass\":\"");
      if (sIdx != -1 && pIdx != -1) {
        int sEnd = content.indexOf("\"", sIdx + 8);
        int pEnd = content.indexOf("\"", pIdx + 8);
        if (sEnd != -1 && pEnd != -1) {
          savedSsid = content.substring(sIdx + 8, sEnd);
          savedPass = content.substring(pIdx + 8, pEnd);
        }
      }
    }
  }
}

void saveWifiCredentials(const String& ssid, const String& pass) {
  savedSsid = ssid;
  savedPass = pass;
  File f = LittleFS.open(FILE_WIFI, FILE_WRITE);
  if (f) {
    f.printf("{\"ssid\":\"%s\",\"pass\":\"%s\"}", ssid.c_str(), pass.c_str());
    f.flush();
    f.close();
  }
  addEventLog("WIFI", "Credenciais atualizadas para SSID: " + ssid, 0);
}

void clearWifiCredentials() {
  savedSsid = "";
  savedPass = "";
  if (LittleFS.exists(FILE_WIFI)) {
    LittleFS.remove(FILE_WIFI);
  }
  addEventLog("WIFI", "Credenciais Wi-Fi limpas da Flash.", 1);
}

void loadMetadata() {
  if (LittleFS.exists(FILE_META)) {
    File f = LittleFS.open(FILE_META, FILE_READ);
    if (f && f.size() == sizeof(RingMeta)) {
      RingMeta tempMeta;
      f.read((uint8_t*)&tempMeta, sizeof(RingMeta));
      f.close();

      uint32_t calcCrc = calculateCrc32((const uint8_t*)&tempMeta, sizeof(uint32_t) * 2);
      if (tempMeta.crc32 == calcCrc && tempMeta.head_index < RING_BUFFER_SIZE) {
        ringMeta = tempMeta;
        return;
      }
      Serial.println("[STORAGE] Aviso: CRC de metadados inconsistente. Restaurando padrao.");
    }
    if (f) f.close();
  }
  ringMeta.head_index = 0;
  ringMeta.total_saved = 0;
  saveMetadataNow();
}

void saveMetadataNow() {
  ringMeta.crc32 = calculateCrc32((const uint8_t*)&ringMeta, sizeof(uint32_t) * 2);
  File mFile = LittleFS.open(FILE_META, FILE_WRITE);
  if (mFile) {
    mFile.write((const uint8_t*)&ringMeta, sizeof(RingMeta));
    mFile.flush();
    mFile.close();
  }
}

int getPendingBatchCount() {
  return ramBatchCount;
}

void flushRamBatch() {
  if (ramBatchCount == 0) return;

  File f = LittleFS.open(FILE_BUFFER, "r+");
  if (!f) {
    f = LittleFS.open(FILE_BUFFER, FILE_WRITE);
    if (f) {
      StorageHeader header;
      header.magic = STORAGE_MAGIC_HEADER;
      header.version = STORAGE_FORMAT_VERSION;
      header.record_size = sizeof(SensorData);
      header.capacity = RING_BUFFER_SIZE;
      header.reserved = 0;
      f.write((const uint8_t*)&header, sizeof(StorageHeader));
    }
  }

  if (f) {
    for (int i = 0; i < ramBatchCount; i++) {
      unsigned long pos = sizeof(StorageHeader) + ((unsigned long)ringMeta.head_index * sizeof(SensorData));
      f.seek(pos, SeekSet);
      f.write((const uint8_t*)&ramBatch[i], sizeof(SensorData));
      ringMeta.head_index = (ringMeta.head_index + 1) % RING_BUFFER_SIZE;
      ringMeta.total_saved++;
    }
    f.flush();
    f.close();

    ramBatchCount = 0;
    saveMetadataNow();
  }
}

void saveSample(const SensorData &data) {
  ramBatch[ramBatchCount++] = data;

  if (ramBatchCount >= RAM_BATCH_SIZE) {
    flushRamBatch();
  }
}

void resetBufferStorage() {
  ramBatchCount = 0;
  ringMeta.head_index = 0;
  ringMeta.total_saved = 0;
  saveMetadataNow();

  if (LittleFS.exists(FILE_BUFFER)) {
    LittleFS.remove(FILE_BUFFER);
  }

  File f = LittleFS.open(FILE_BUFFER, FILE_WRITE);
  if (f) {
    StorageHeader header;
    header.magic = STORAGE_MAGIC_HEADER;
    header.version = STORAGE_FORMAT_VERSION;
    header.record_size = sizeof(SensorData);
    header.capacity = RING_BUFFER_SIZE;
    header.reserved = 0;
    f.write((const uint8_t*)&header, sizeof(StorageHeader));
    f.flush();
    f.close();
  }
  addEventLog("STORAGE", "Buffer circular de 12 horas zerado e reinicializado.", 1);
}

void setSystemStandby(bool enableStandby, bool notifyTelegram) {
  isSystemStandby = enableStandby;
  saveAudioAndPowerConfig();

  if (enableStandby) {
    flushRamBatch();
    saveMetadataNow();
    stopAllSounds();
    pwmWriteLed(0);
    if (!isApMode) {
      WiFi.setSleep(true);
    }
    addEventLog("ENERGIA", "Modo Standby Remoto ativado.", 0);
    if (notifyTelegram) {
      sendTelegramMessage(
        "🌙 <b>ESTAÇÃO EM MODO STANDBY (PAUSADA)</b>\n\n"
        "• Aquisição de sensores: <b>PAUSADA</b>\n"
        "• Gravação Flash: <b>PAUSADA (0% Desgaste)</b>\n"
        "• Lote RAM: <b>SINCRONIZADO</b>\n"
        "• LED e Buzzers (D18 e D27): <b>SILENCIADOS / APAGADOS</b>\n"
        "• Economia Wi-Fi: <b>HABILITADA</b>\n\n"
        "💡 <i>Envie</i> /on <i>ou</i> /ligar <i>para religar.</i>");
    }
  } else {
    if (!isApMode) {
      WiFi.setSleep(false);
    }
    lastSampleTime = millis();
    triggerManualBeepTest(selectedBeepDevice, 80);
    addEventLog("ENERGIA", "Estacao restabelecida ao modo operacional.", 0);
    if (notifyTelegram) {
      sendTelegramMessage(
        "☀️ <b>ESTAÇÃO OPERACIONAL E ATIVA</b>\n\n"
        "• Ciclo de amostragem: <b>ATIVO (5s)</b>\n"
        "• Buffer Circular LittleFS (12h): <b>GRAVANDO (Batching Ativo)</b>\n"
        "• Sensores de Alarme (Fogo/Chuva/Gás): <b>ARMADOS</b>\n"
        "• Local: <b>" + locationInfo + "</b>");
    }
  }
}

void enterSafeDeepSleep() {
  addEventLog("ENERGIA", "Preparando para Deep Sleep fisico...", 1);
  flushRamBatch();
  saveMetadataNow();
  LittleFS.end();

  pwmStop3V();
  digitalTone5V(false);
  pwmWriteLed(0);

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  Serial.println("[ENERGIA] Flash desmontada. Entrando em Deep Sleep.");
  delay(100);
  esp_deep_sleep_start();
}

AverageStats calculateAverages(int maxSamples) {
  AverageStats st = {0.0f, 0.0f, 0.0f, 999.0f, -999.0f, 999.0f, -999.0f, 0};
  
  float sumT = 0.0f, sumH = 0.0f, sumMq = 0.0f;
  int validCount = 0;

  for (int i = ramBatchCount - 1; i >= 0 && validCount < maxSamples; i--) {
    SensorData &s = ramBatch[i];
    if (s.data_quality != DATA_INVALID && s.temperature > -35.0f && s.temperature < 85.0f) {
      sumT += s.temperature;
      sumH += s.humidity;
      sumMq += s.mq135_ppm_comp;
      if (s.temperature < st.minTemp) st.minTemp = s.temperature;
      if (s.temperature > st.maxTemp) st.maxTemp = s.temperature;
      if (s.humidity < st.minHum) st.minHum = s.humidity;
      if (s.humidity > st.maxHum) st.maxHum = s.humidity;
      validCount++;
    }
  }

  if (validCount < maxSamples && ringMeta.total_saved > 0) {
    File f = LittleFS.open(FILE_BUFFER, FILE_READ);
    if (f) {
      int available = (ringMeta.total_saved < RING_BUFFER_SIZE) ? ringMeta.total_saved : RING_BUFFER_SIZE;
      int toRead = min(available, maxSamples - validCount);

      for (int i = 0; i < toRead; i++) {
        int idx = (int)ringMeta.head_index - 1 - i;
        if (idx < 0) idx += RING_BUFFER_SIZE;

        unsigned long pos = sizeof(StorageHeader) + ((unsigned long)idx * sizeof(SensorData));
        f.seek(pos, SeekSet);

        SensorData s;
        if (f.read((uint8_t*)&s, sizeof(SensorData)) == sizeof(SensorData)) {
          if (s.data_quality != DATA_INVALID && s.temperature > -35.0f && s.temperature < 85.0f) {
            sumT += s.temperature;
            sumH += s.humidity;
            sumMq += s.mq135_ppm_comp;
            if (s.temperature < st.minTemp) st.minTemp = s.temperature;
            if (s.temperature > st.maxTemp) st.maxTemp = s.temperature;
            if (s.humidity < st.minHum) st.minHum = s.humidity;
            if (s.humidity > st.maxHum) st.maxHum = s.humidity;
            validCount++;
          }
        }
      }
      f.close();
    }
  }

  if (validCount > 0) {
    st.avgTemp = sumT / validCount;
    st.avgHum  = sumH / validCount;
    st.avgMq   = sumMq / validCount;
    st.count   = validCount;
  } else {
    st.minTemp = 0.0f;
    st.maxTemp = 0.0f;
    st.minHum  = 0.0f;
    st.maxHum  = 0.0f;
  }
  return st;
}

size_t exportStorageAsCsv(Print &output) {
  flushRamBatch();
  
  size_t bytesSent = 0;
  bytesSent += output.print("Data_Hora;Timestamp_Sec;Temperatura_C;Umidade_Pct;Gas_PPM_Comp;IQA;Sensacao_C;Orvalho_C;dT_dt_C_min;MQ_ADC;Chuva_ADC;Chuva_Det;Fogo_Det;Risco;TinyML_Estado;Qualidade;Localizacao;Latitude;Longitude\r\n");

  if (ringMeta.total_saved == 0) return bytesSent;

  File f = LittleFS.open(FILE_BUFFER, FILE_READ);
  if (!f) return bytesSent;

  int available = (ringMeta.total_saved < RING_BUFFER_SIZE) ? ringMeta.total_saved : RING_BUFFER_SIZE;
  int startIdx = (ringMeta.total_saved < RING_BUFFER_SIZE) ? 0 : ringMeta.head_index;

  time_t nowEpoch = 0;
  bool hasRealTime = false;
  uint32_t currentUptime = millis() / 1000UL;

  if (ntpSynced) {
    time(&nowEpoch);
    if (nowEpoch > 1700000000) {
      hasRealTime = true;
    }
  }

  String locName = (isApMode || !isStationConnected() || locationCityRegion == "sem local") ? "sem local" : locationCityRegion;
  String latVal  = (isApMode || !isStationConnected() || locationLatitude == "sem local")   ? "sem local" : locationLatitude;
  String lonVal  = (isApMode || !isStationConnected() || locationLongitude == "sem local")  ? "sem local" : locationLongitude;

  for (int i = 0; i < available; i++) {
    int idx = (startIdx + i) % RING_BUFFER_SIZE;
    unsigned long pos = sizeof(StorageHeader) + ((unsigned long)idx * sizeof(SensorData));
    f.seek(pos, SeekSet);

    SensorData s;
    if (f.read((uint8_t*)&s, sizeof(SensorData)) == sizeof(SensorData)) {
      char dateStr[32];
      if (hasRealTime && s.session_id == currentSessionId && currentUptime >= s.uptime_sec) {
        time_t sampleEpoch = nowEpoch - (currentUptime - s.uptime_sec);
        struct tm timeinfo;
        localtime_r(&sampleEpoch, &timeinfo);
        strftime(dateStr, sizeof(dateStr), "%d/%m/%Y %H:%M:%S", &timeinfo);
      } else {
        unsigned long sec = s.uptime_sec;
        unsigned long h = sec / 3600UL;
        unsigned long m = (sec % 3600UL) / 60UL;
        unsigned long sc = sec % 60UL;
        snprintf(dateStr, sizeof(dateStr), "Uptime %02lu:%02lu:%02lu", h, m, sc);
      }

      float ppmVal = s.mq135_ppm_comp;
      if (ppmVal < 50.0f && s.mq135_raw > 0) {
        ppmVal = calculateRawPPM(s.mq135_raw);
      }
      int sampleAqi = calculateAQI(ppmVal);

      char line[320];
      snprintf(line, sizeof(line),
               "%s;%lu;%.1f;%.1f;%.1f;%d;%.1f;%.1f;%.2f;%d;%d;%u;%u;%s;%s;%u;%s;%s;%s\r\n",
               dateStr,
               s.uptime_sec,
               s.temperature,
               s.humidity,
               ppmVal,
               sampleAqi,
               s.heat_index,
               s.dew_point,
               s.temp_rate_minute,
               s.mq135_raw,
               s.rain_raw,
               s.rain_detected,
               s.flame_detected,
               getRiskLevelName((RiskLevel)s.risk_level),
               AtalaiaModel::getStateName((MLState)s.ml_state),
               s.data_quality,
               locName.c_str(),
               latVal.c_str(),
               lonVal.c_str());
      bytesSent += output.print(line);
    }
  }
  f.close();
  return bytesSent;
}