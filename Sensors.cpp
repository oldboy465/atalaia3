#include "Sensors.h"
#include "Actuators.h"
#include "Storage.h"
#include "TelegramBot.h"
#include "DisplayManager.h"
#include <math.h>

DHT dht(PIN_DHT, DHT22);

static float lastValidTemp = 25.0f;
static float lastValidHum  = 50.0f;
static bool  hasFirstValidDht = false;

// Buffer circular deslizante para cálculo da taxa de variação dT/dt (60 segundos)
static float tempHistoryWindow[THERMAL_WINDOW_SAMPLES];
static int   tempHistoryIndex = 0;
static bool  tempHistoryFilled = false;

// Integrador anti-ruído para sensor de chama óptico D33
static unsigned long flameIntegratorStart = 0;
static bool flameConfirmedState = false;
static unsigned long lastFlameHoldTime = 0;

void initSensors() {
  pinMode(PIN_FLAME_DO, INPUT_PULLUP);
  dht.begin();
  for (int i = 0; i < THERMAL_WINDOW_SAMPLES; i++) {
    tempHistoryWindow[i] = 25.0f;
  }
}

int readAdcOversampled(uint8_t pin, int samples) {
  analogRead(pin);
  delayMicroseconds(60);

  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delayMicroseconds(80);
  }
  return (int)(sum / samples);
}

bool updateFlameStatusNonBlocking() {
  int pinState = digitalRead(PIN_FLAME_DO);
  unsigned long now = millis();

  // Avaliação com suporte a polaridade ativa (Active LOW ou Active HIGH)
  bool isTriggered = flameActiveLow ? (pinState == LOW) : (pinState == HIGH);

  if (isTriggered) {
    if (flameIntegratorStart == 0) {
      flameIntegratorStart = now;
    } else if (now - flameIntegratorStart >= 30UL) { // Integrador ágil para luz e fogo
      flameConfirmedState = true;
      lastFlameHoldTime = now;
    }
  } else {
    flameIntegratorStart = 0;
    if (flameConfirmedState && (now - lastFlameHoldTime >= 400UL)) {
      flameConfirmedState = false;
    }
  }
  return flameConfirmedState;
}

bool readDHT(float &temp, float &hum, DataQuality &quality) {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    quality = DATA_INVALID;
    if (hasFirstValidDht) {
      temp = lastValidTemp;
      hum  = lastValidHum;
    } else {
      temp = 25.0f;
      hum  = 50.0f;
    }
    return false;
  }

  if (t < -20.0f || t > 85.0f || h < 0.0f || h > 100.0f) {
    quality = DATA_INVALID;
    temp = lastValidTemp;
    hum  = lastValidHum;
    return false;
  }

  if (hasFirstValidDht) {
    float deltaT = fabs(t - lastValidTemp);
    float deltaH = fabs(h - lastValidHum);
    if (deltaT > 8.0f || deltaH > 25.0f) {
      quality = DATA_SUSPECT;
    } else {
      quality = DATA_VALID;
    }
  } else {
    quality = DATA_VALID;
    hasFirstValidDht = true;
  }

  temp = t;
  hum  = h;
  lastValidTemp = t;
  lastValidHum  = h;
  return (quality != DATA_INVALID);
}

float getCorrectionFactorMQ135(float tempC, float humPct) {
  float t = constrain(tempC, -10.0f, 60.0f);
  float h = constrain(humPct, 10.0f, 95.0f);
  float factor = 1.898f - (0.0242f * t) - (0.0035f * h) + (0.00018f * t * (h / 100.0f));
  return constrain(factor, 0.35f, 2.2f);
}

float calculateCompensatedPPM(int rawAdc, float tempC, float humPct, float &outRsRoRatio) {
  if (rawAdc <= 45) {
    outRsRoRatio = 1.0f;
    return 400.0f;
  }

  float vOut = ((float)rawAdc / 4095.0f) * 3.3f;
  if (vOut >= 3.20f) vOut = 3.20f;
  if (vOut <= 0.08f) {
    outRsRoRatio = 1.0f;
    return 400.0f;
  }

  float rs = 10.0f * (3.3f - vOut) / vOut;
  float r0 = 24.0f;
  float rawRatio = rs / r0;

  float cf = getCorrectionFactorMQ135(tempC, humPct);
  float correctedRatio = rawRatio / cf;
  if (correctedRatio <= 0.04f) correctedRatio = 0.04f;

  outRsRoRatio = correctedRatio;
  
  float ppm = 400.0f * pow(correctedRatio, -1.72f);
  return constrain(ppm, 350.0f, 5000.0f);
}

float calculateRawPPM(int rawAdc) {
  float dummyRatio = 0.0f;
  return calculateCompensatedPPM(rawAdc, 20.0f, 65.0f, dummyRatio);
}

int calculateAQI(float ppm) {
  float aqi = 0.0f;
  if (ppm <= 400.0f) {
    aqi = 10.0f + ((ppm - 350.0f) / 50.0f) * 15.0f;
  } else if (ppm <= 600.0f) {
    aqi = 25.0f + ((ppm - 400.0f) / 200.0f) * 25.0f;
  } else if (ppm <= 1000.0f) {
    aqi = 50.0f + ((ppm - 600.0f) / 400.0f) * 50.0f;
  } else if (ppm <= 1500.0f) {
    aqi = 100.0f + ((ppm - 1000.0f) / 50.0f) * 50.0f;
  } else if (ppm <= 2500.0f) {
    aqi = 150.0f + ((ppm - 1500.0f) / 1000.0f) * 50.0f;
  } else {
    aqi = 200.0f + ((ppm - 2500.0f) / 2500.0f) * 300.0f;
  }
  return constrain((int)round(aqi), 10, 500);
}

String getAQICategory(int aqi) {
  if (aqi <= 25)  return "EXCELENTE (Puro)";
  if (aqi <= 50)  return "BOM (Normal)";
  if (aqi <= 100) return "MODERADO (Atencao)";
  if (aqi <= 150) return "RUIM (Prejudicial)";
  if (aqi <= 200) return "MUITO RUIM (Insalubre)";
  return "CRITICO / PERIGOSO";
}

String getAirQualityLabel(float ppm) {
  int aqi = calculateAQI(ppm);
  return getAQICategory(aqi);
}

float calculateHeatIndex(float tempC, float humPct) {
  if (tempC <= 0.0f || humPct <= 0.0f) return tempC;
  float tf = (tempC * 9.0f / 5.0f) + 32.0f;
  float hi = 0.5f * (tf + 61.0f + ((tf - 68.0f) * 1.2f) + (humPct * 0.094f));
  if ((hi + tf) * 0.5f >= 80.0f) {
    hi = -42.379f + 2.04901523f * tf + 10.14333127f * humPct
         - 0.22475541f * tf * humPct - 0.00683783f * tf * tf
         - 0.05481717f * humPct * humPct + 0.00122874f * tf * tf * humPct
         + 0.00085282f * tf * humPct * humPct - 0.00000199f * tf * tf * humPct * humPct;
  }
  return (hi - 32.0f) * 5.0f / 9.0f;
}

float calculateDewPoint(float tempC, float humPct) {
  if (humPct <= 0.0f || tempC <= -50.0f) return tempC;
  float a = 17.27f;
  float b = 237.7f;
  float alpha = ((a * tempC) / (b + tempC)) + log(max(1.0f, humPct) / 100.0f);
  return (b * alpha) / (a - alpha);
}

float computeThermalDerivative(float currentTemp) {
  tempHistoryWindow[tempHistoryIndex] = currentTemp;
  tempHistoryIndex = (tempHistoryIndex + 1) % THERMAL_WINDOW_SAMPLES;
  if (tempHistoryIndex == 0) {
    tempHistoryFilled = true;
  }

  int oldestIndex = tempHistoryIndex;
  float oldestTemp = tempHistoryFilled ? tempHistoryWindow[oldestIndex] : tempHistoryWindow[0];
  return (currentTemp - oldestTemp);
}

RiskLevel evaluateSystemRisk(float ppm, float tempRate, bool flame, bool rain) {
  if (flame) {
    return RISK_CRITICO;
  }
  if (ppm >= gasAlertThresholdPpm || tempRate >= THERMAL_RATE_TRIGGER_C_MIN) {
    return RISK_ALERTA;
  }
  if (ppm >= (gasAlertThresholdPpm - GAS_HYSTERESIS_DELTA) || rain) {
    return RISK_ATENCAO;
  }
  return RISK_NORMAL;
}

void executePeriodicSampling() {
  if (isSystemStandby) return;

  unsigned long now = millis();
  bool isWarmedUp = (now >= warmupDurationMs);

  // 1. Aquisição com Validação de Integridade do DHT22
  float t = 0.0f, h = 0.0f;
  DataQuality quality = DATA_VALID;
  readDHT(t, h, quality);

  currentData.temperature  = t;
  currentData.humidity     = h;
  currentData.data_quality = (uint8_t)quality;

  // 2. Análise Psicrométrica e Derivada Térmica (dT/dt)
  currentData.heat_index       = calculateHeatIndex(t, h);
  currentData.dew_point        = calculateDewPoint(t, h);
  currentData.temp_rate_minute = computeThermalDerivative(t);

  // 3. Leituras Analógicas dos Sensores com Sobreamostragem de 16 Ciclos
  int mqVal   = readAdcOversampled(PIN_MQ135_AO, 16);
  int rainVal = readAdcOversampled(PIN_RAIN_AO, 16);
  bool rainDetected = (rainVal < RAIN_THRESHOLD);

  currentData.mq135_raw     = mqVal;
  currentData.rain_raw      = rainVal;
  currentData.rain_detected = rainDetected ? 1 : 0;

  // 4. Compensação Dinâmica de Gás e AQI
  float rsRoRatio = 1.0f;
  float ppmComp   = calculateCompensatedPPM(mqVal, t, h, rsRoRatio);
  currentData.mq135_ppm_comp = ppmComp;

  // 5. Atualização da série temporal para mini-gráficos no display
  // Alimentação com o valor calibrado do sensor MQ (ADC bruto compensado/filtrado)
  pushTelemetryToOledHistory(t, h, (float)mqVal);

  // 6. Inferência Nativa TinyML na Borda
  MLState predictedState = AtalaiaModel::predict(t, h, rsRoRatio, rainVal, currentData.temp_rate_minute);
  currentData.ml_state = (uint8_t)predictedState;

  // 7. Avaliação da Máquina de Estados de Risco
  bool isFlame = (currentData.flame_detected == 1);
  RiskLevel rLevel = evaluateSystemRisk(ppmComp, currentData.temp_rate_minute, isFlame, rainDetected);
  currentData.risk_level = (uint8_t)rLevel;

  // 8. Checksum e Dados Temporais
  currentData.uptime_sec = now / 1000UL;
  currentData.session_id = currentSessionId;
  
  uint8_t chk = 0;
  uint8_t* bytePtr = (uint8_t*)&currentData;
  for (size_t b = 0; b < sizeof(SensorData) - 3; b++) {
    chk ^= bytePtr[b];
  }
  currentData.checksum = chk;

  // 9. Gravação com Proteção Wear Leveling na Flash
  saveSample(currentData);

  // 10. Feedback Atómico de Amostragem (12ms)
  triggerCollectionFeedback(BEEP_TICK_MS);

  // 11. Gestão Sonora de Chuva Não-Bloqueante (Não desliga se teste manual estiver ativo)
  if (rainDetected && !isFlame) {
    startRainMelody(false);
  } else if (!rainDetected && !isManualRainTest) {
    stopRainMelody();
  }

  // 12. Notificações Telegram
  if (rainDetected && (!lastRainNotified || (now - rainAlertCooldown > 45000UL))) {
    addEventLog("CHUVA", "Precipitacao confirmada (ADC=" + String(rainVal) + ").", 1);
    sendTelegramMessage(
      "🌧 <b>ALERTA DE CHUVA — ATALAIA 3</b>\n"
      "Superfície condutora com água detectada!\n"
      "• Temp: " + String(currentData.temperature, 1) + " °C | Umid: " + String(currentData.humidity, 1) + " %\n"
      "• Leitura ADC: " + String(rainVal) + "\n"
      "• Modelo TinyML: <b>" + String(AtalaiaModel::getStateName(predictedState)) + "</b>\n"
      "• Local: " + locationInfo,
      true);
    lastRainNotified = true;
    rainAlertCooldown = now;
  } else if (!rainDetected) {
    lastRainNotified = false;
  }

  // Alerta Preditivo de Gradiente Térmico Anormal (dT/dt)
  if (currentData.temp_rate_minute >= THERMAL_RATE_TRIGGER_C_MIN && !isFlame) {
    if (!lastPreFireNotified || (now - preFireAlertCooldown > 30000UL)) {
      addEventLog("PREDICAO", "Elevacao termica subita: +" + String(currentData.temp_rate_minute, 1) + " C/min", 2);
      sendTelegramMessage(
        "⚠️ <b>ALERTA PREDITIVO — GRADIENTE TÉRMICO ANORMAL</b>\n"
        "• Derivada Térmica: <b>+" + String(currentData.temp_rate_minute, 1) + " °C/min</b>\n"
        "• Temp Atual: " + String(currentData.temperature, 1) + " °C\n"
        "• Diagnóstico TinyML: <b>" + String(AtalaiaModel::getStateName(predictedState)) + "</b>\n"
        "• Risco de Ignição Iminente Detectado!",
        true);
      lastPreFireNotified = true;
      preFireAlertCooldown = now;
    }
  } else if (currentData.temp_rate_minute < 1.0f) {
    lastPreFireNotified = false;
  }

  // Alarme Sonoro e Telemetria de Gás MQ-135 (Não desliga se teste manual estiver ativo)
  if (ppmComp > gasAlertThresholdPpm) {
    if (isWarmedUp) {
      startGasAlarm(false, 0);

      if (!lastGasNotified || (now - gasAlertCooldown > 45000UL)) {
        int curAqi = calculateAQI(ppmComp);
        addEventLog("GAS", "Gas critico (~" + String((int)ppmComp) + " ppm | AQI " + String(curAqi) + ")", 2);
        sendTelegramMessage(
          "⚠️ <b>ALERTA DE GÁS ELEVADO (COMPENSADO) — ATALAIA 3</b>\n"
          "• Índice AQI: <b>" + String(curAqi) + " (" + getAQICategory(curAqi) + ")</b>\n"
          "• Concentração Compensada: <b>~" + String((int)ppmComp) + " ppm</b> (Limiar: " + String((int)gasAlertThresholdPpm) + " ppm)\n"
          "• Fator Higrotérmico Aplicado: " + String(getCorrectionFactorMQ135(t, h), 2) + "\n"
          "• Local: " + locationInfo,
          true);
        lastGasNotified = true;
        gasAlertCooldown = now;
      }
    } else {
      static unsigned long lastWarmupLog = 0;
      if (now - lastWarmupLog > 20000UL) {
        lastWarmupLog = now;
        Serial.printf("[SENSOR-MQ] Pre-aquecendo... Leitura bruta: ~%d ppm (Alertas suspensos).\n", (int)ppmComp);
      }
    }
  } else if (ppmComp <= (gasAlertThresholdPpm - GAS_HYSTERESIS_DELTA)) {
    if (isWarmedUp && !isManualGasTest) {
      stopGasAlarm();
    }
    lastGasNotified = false;
  }
}