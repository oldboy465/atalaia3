#include "Actuators.h"
#include "Storage.h"
#include "DisplayManager.h"

// ============================================================================
// ESTADO INTERNO DOS ATUADORES E CONTROLE DE TEMPORIZAÇÃO (PRESERVAÇÃO INTEGRAL)
// ============================================================================
static SoundRiskMode currentRiskMode = SR_IDLE;
static unsigned long riskTimerNextStep = 0;

// Estado Sirene Fogo Louca (Varredura de alta velocidade 800Hz - 3200Hz)
static int fireSirenFreq = 800;
static bool fireSirenUp = true;
bool isManualFireTest = false;
static unsigned long manualFireEndTime = 0;
static bool fireStrobeState = false;

// Estado Alarme Gás (Pulsos de onda quadrada em quadratura)
bool isManualGasTest = false;
static unsigned long manualGasEndTime = 0;
static bool gasPulseState = false;

// Partitura Completa do Jingle Bells gravada integralmente na Flash da ESP32
static const int fullChristmasMelody[] = {
  // Frase 1: Jingle bells, jingle bells, jingle all the way
  659, 659, 659,
  659, 659, 659,
  659, 784, 523, 587,
  659,
  // Oh what fun it is to ride in a one horse open sleigh, hey!
  698, 698, 698, 698,
  698, 659, 659, 659, 659,
  659, 587, 587, 659,
  587, 784,
  // Frase 2: Jingle bells, jingle bells, jingle all the way
  659, 659, 659,
  659, 659, 659,
  659, 784, 523, 587,
  659,
  // Oh what fun it is to ride in a one horse open sleigh!
  698, 698, 698, 698,
  698, 659, 659, 659, 659,
  784, 784, 698, 587,
  523
};

static const int fullChristmasDurations[] = {
  // Frase 1
  175, 175, 350,
  175, 175, 350,
  175, 175, 175, 175,
  500,
  175, 175, 175, 175,
  175, 175, 175, 90, 90,
  175, 175, 175, 175,
  350, 350,
  // Frase 2
  175, 175, 350,
  175, 175, 350,
  175, 175, 175, 175,
  500,
  175, 175, 175, 175,
  175, 175, 175, 90, 90,
  175, 175, 175, 175,
  600
};

static const int totalChristmasNotes = sizeof(fullChristmasMelody) / sizeof(fullChristmasMelody[0]);

// Estado Melodia de Chuva não-bloqueante
static int  jingleNoteIndex   = 0;
static bool jingleNotePlaying = false;
bool isManualRainTest  = false;
static unsigned long manualRainEndTime = 0;

// Heartbeat e Flash do LED Verde
static int  breathBrightness = 0;
static int  breathStep       = 3;
static unsigned long lastBreathTime = 0;

static bool ledFlashActive = false;
static unsigned long ledFlashEndTime = 0;

void pwmInit() {
  pinMode(PIN_BUZZER_5V, OUTPUT);
  digitalWrite(PIN_BUZZER_5V, LOW);

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcAttachChannel(PIN_LED_VERDE,  PWM_FREQ_LED, PWM_RES_BITS, LEDC_CH_LED);
  ledcAttachChannel(PIN_BUZZER_3V3, 2700,         PWM_RES_BITS, LEDC_CH_BUZZER3V);
#else
  ledcSetup(LEDC_CH_LED, PWM_FREQ_LED, PWM_RES_BITS);
  ledcAttachPin(PIN_LED_VERDE, LEDC_CH_LED);

  ledcSetup(LEDC_CH_BUZZER3V, 2700, PWM_RES_BITS);
  ledcAttachPin(PIN_BUZZER_3V3, LEDC_CH_BUZZER3V);
#endif

  pwmWriteLed(0);
  pwmStop3V();
  digitalTone5V(false);
}

void pwmWriteLed(uint32_t duty) {
  uint32_t clamped = constrain(duty, 0, 255);
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcWrite(PIN_LED_VERDE, clamped);
#else
  ledcWrite(LEDC_CH_LED, clamped);
#endif
}

void pwmTone3V(uint32_t freq) {
  if (!masterSoundEnabled || freq == 0) {
    pwmStop3V();
    return;
  }
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcWriteTone(PIN_BUZZER_3V3, freq);
#else
  ledcWriteTone(LEDC_CH_BUZZER3V, freq);
#endif
}

void pwmStop3V() {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  ledcWriteTone(PIN_BUZZER_3V3, 0);
  ledcWrite(PIN_BUZZER_3V3, 0);
#else
  ledcWriteTone(LEDC_CH_BUZZER3V, 0);
  ledcWrite(LEDC_CH_BUZZER3V, 0);
#endif
}

void digitalTone5V(bool state) {
  if (!masterSoundEnabled && state) {
    digitalWrite(PIN_BUZZER_5V, LOW);
    return;
  }
  digitalWrite(PIN_BUZZER_5V, state ? HIGH : LOW);
}

void setLedMode(LedMode mode) {
  currentLedMode = mode;
  ledFlashActive = false;
  if (currentLedMode == LED_MODE_FORCE_ON) {
    pwmWriteLed(255);
  } else if (currentLedMode == LED_MODE_FORCE_OFF) {
    pwmWriteLed(0);
  }
}

// Bip atómico seco de 12ms a 15ms
void triggerCollectionFeedback(unsigned long beepMs) {
  if (isSystemStandby || !masterSoundEnabled || !collectionBeepEnabled) return;
  if (currentRiskMode != SR_IDLE) return;

  unsigned long ms = constrain(beepMs, 8UL, 25UL);

  if (collectionFlashEnabled && (currentLedMode == LED_MODE_AUTO_HEARTBEAT)) {
    pwmWriteLed(255);
  }

  if (selectedBeepDevice == BEEP_DEV_BUZZER_3V) {
    pwmTone3V(3200);
    delayMicroseconds(ms * 1000UL);
    pwmStop3V();
  } else if (selectedBeepDevice == BEEP_DEV_ALL) {
    pwmTone3V(3200);
    digitalWrite(PIN_BUZZER_5V, HIGH);
    delayMicroseconds(ms * 1000UL);
    digitalWrite(PIN_BUZZER_5V, LOW);
    pwmStop3V();
  } else {
    digitalWrite(PIN_BUZZER_5V, HIGH);
    delayMicroseconds(ms * 1000UL);
    digitalWrite(PIN_BUZZER_5V, LOW);
  }

  if (collectionFlashEnabled && (currentLedMode == LED_MODE_AUTO_HEARTBEAT)) {
    ledFlashActive = true;
    ledFlashEndTime = millis() + 35UL;
  }
}

void playFullChristmasMelody() {
  if (!masterSoundEnabled) return;
  addEventLog("AUDIO", "Executando partitura completa de Natal na ESP32...", 0);

  // Aciona animação de Natal no OLED durante a melodia
  triggerDisplayChristmasAnimation(14000);

  currentRiskMode = SR_IDLE;
  for (int i = 0; i < totalChristmasNotes; i++) {
    if (!masterSoundEnabled || currentData.flame_detected == 1) {
      break;
    }
    pwmTone3V(fullChristmasMelody[i]);
    if (collectionFlashEnabled && (currentLedMode == LED_MODE_AUTO_HEARTBEAT)) {
      pwmWriteLed(180);
    }
    delay(fullChristmasDurations[i]);

    pwmStop3V();
    if (collectionFlashEnabled && (currentLedMode == LED_MODE_AUTO_HEARTBEAT)) {
      pwmWriteLed(0);
    }
    delay(40);
  }
  pwmStop3V();
}

void triggerManualBeepTest(BeepDevice targetDevice, unsigned long beepMs) {
  if (!masterSoundEnabled) return;

  unsigned long ms = constrain(beepMs, 10UL, 35UL);

  if (currentLedMode == LED_MODE_AUTO_HEARTBEAT) {
    pwmWriteLed(255);
  }

  if (targetDevice == BEEP_DEV_BUZZER_3V) {
    pwmTone3V(3000);
    delayMicroseconds(ms * 1000UL);
    pwmStop3V();
    addEventLog("TESTE", "Bip curto Buzzer 3.3V (D18) executado.", 0);
  } else if (targetDevice == BEEP_DEV_ALL) {
    pwmTone3V(3000);
    digitalWrite(PIN_BUZZER_5V, HIGH);
    delayMicroseconds(ms * 1000UL);
    digitalWrite(PIN_BUZZER_5V, LOW);
    pwmStop3V();
    addEventLog("TESTE", "Bip curto Geral (D18 + D27) executado.", 0);
  } else {
    digitalWrite(PIN_BUZZER_5V, HIGH);
    delayMicroseconds(ms * 1000UL);
    digitalWrite(PIN_BUZZER_5V, LOW);
    addEventLog("TESTE", "Bip curto Buzzer 5V (D27) executado.", 0);
  }

  if (currentLedMode == LED_MODE_AUTO_HEARTBEAT) {
    ledFlashActive = true;
    ledFlashEndTime = millis() + 35UL;
  }
}

// Sirene de Incêndio: Varredura 800Hz-3200Hz no 3.3V (D18) e estrobo pulsante no 5V (D27)
void startSiren(bool isManualTest, unsigned long manualDurationMs) {
  if (isManualTest) {
    isManualFireTest = true;
    manualFireEndTime = millis() + (manualDurationMs > 0 ? manualDurationMs : 3500UL);
    addEventLog("TESTE", "Sirene de Fogo (Histerica) acionada.", 1);
    Serial.printf("[AUDIO] Sirene disparada em modo TESTE MANUAL (Duracao: %lu ms).\n", (manualDurationMs > 0 ? manualDurationMs : 3500UL));
  } else {
    Serial.println("[AUDIO] Sirene disparada por EMERGENCIA AUTOMATICA (Fogo Detectado).");
  }

  // Se Som Mestre estiver desativado, não toca
  if (!masterSoundEnabled) {
    Serial.println("[AUDIO] Sirene bloqueada: Som Mestre desligado (/sound_off).");
    return;
  }

  // Testes manuais tocam mesmo que o alarme automático esteja desarmado
  if (!riskSirensEnabled && !isManualTest) {
    return;
  }

  currentRiskMode = SR_FIRE_SIREN;
  fireSirenFreq = 800;
  fireSirenUp = true;
  fireStrobeState = true;
  digitalTone5V(true);
  pwmTone3V(fireSirenFreq);
  riskTimerNextStep = millis();
}

void stopSiren() {
  if (currentRiskMode == SR_FIRE_SIREN) {
    currentRiskMode = SR_IDLE;
    isManualFireTest = false;
    pwmStop3V();
    digitalTone5V(false);
  }
}

void startGasAlarm(bool isManualTest, unsigned long manualDurationMs) {
  if (isManualTest) {
    isManualGasTest = true;
    manualGasEndTime = millis() + (manualDurationMs > 0 ? manualDurationMs : 3500UL);
    addEventLog("TESTE", "Sirene de Gas acionada.", 1);
    Serial.printf("[AUDIO] Alarme de Gas disparado em TESTE MANUAL (Duracao: %lu ms).\n", (manualDurationMs > 0 ? manualDurationMs : 3500UL));
  }

  if (!masterSoundEnabled) return;
  if (!riskSirensEnabled && !isManualTest) return;
  if (currentRiskMode == SR_FIRE_SIREN && currentData.flame_detected == 1) return;

  currentRiskMode = SR_GAS_ALARM;
  gasPulseState = true;
  pwmTone3V(3100);
  digitalTone5V(true);
  riskTimerNextStep = millis() + 140;
}

void stopGasAlarm() {
  if (currentRiskMode == SR_GAS_ALARM) {
    currentRiskMode = SR_IDLE;
    isManualGasTest = false;
    pwmStop3V();
    digitalTone5V(false);
  }
}

void startRainMelody(bool isManualTest) {
  if (isManualTest) {
    isManualRainTest = true;
    manualRainEndTime = millis() + 14000UL;
    triggerDisplayChristmasAnimation(14000);
    playFullChristmasMelody();
    isManualRainTest = false;
    return;
  }
  if (!masterSoundEnabled) return;
  if (!riskSirensEnabled && !isManualTest) return;
  if (currentRiskMode == SR_FIRE_SIREN || currentRiskMode == SR_GAS_ALARM) return;

  currentRiskMode = SR_RAIN_MELODY;
  jingleNoteIndex = 0;
  jingleNotePlaying = false;
  riskTimerNextStep = millis();
  triggerDisplayChristmasAnimation(8000);
}

void stopRainMelody() {
  if (currentRiskMode == SR_RAIN_MELODY) {
    currentRiskMode = SR_IDLE;
    isManualRainTest = false;
    pwmStop3V();
  }
}

void stopAllSounds() {
  isManualFireTest = false;
  isManualGasTest  = false;
  isManualRainTest = false;
  currentRiskMode  = SR_IDLE;

  pwmStop3V();
  digitalTone5V(false);
  addEventLog("AUDIO", "Atuadores sonoros silenciados.", 0);
}

void updateBuzzersAndLed() {
  unsigned long now = millis();

  // 1. Temporizadores de testes manuais
  if (isManualFireTest && now >= manualFireEndTime) {
    isManualFireTest = false;
    if (currentData.flame_detected == 0) {
      stopSiren();
    }
  }

  if (isManualGasTest && now >= manualGasEndTime) {
    isManualGasTest = false;
    stopGasAlarm();
  }

  if (isManualRainTest && now >= manualRainEndTime) {
    isManualRainTest = false;
    stopRainMelody();
  }

  // 2. LED Verde Heartbeat
  if (isSystemStandby) {
    pwmWriteLed(0);
  } else if (currentLedMode == LED_MODE_FORCE_ON) {
    pwmWriteLed(255);
  } else if (currentLedMode == LED_MODE_FORCE_OFF) {
    pwmWriteLed(0);
  } else {
    if (ledFlashActive) {
      if (now >= ledFlashEndTime) {
        ledFlashActive = false;
      } else {
        pwmWriteLed(255);
      }
    } else {
      if (now - lastBreathTime >= 25) {
        lastBreathTime = now;
        breathBrightness += breathStep;
        if (breathBrightness <= 2) {
          breathBrightness = 2;
          breathStep = 3;
        } else if (breathBrightness >= 160) {
          breathBrightness = 160;
          breathStep = -3;
        }
        pwmWriteLed(breathBrightness);
      }
    }
  }

  // 3. Bloqueio geral quando Som Mestre desligado
  if (!masterSoundEnabled) {
    if (currentRiskMode != SR_IDLE) {
      pwmStop3V();
      digitalTone5V(false);
      currentRiskMode = SR_IDLE;
    }
    return;
  }

  // 4. Máquina de Estados dos Alarmes Sonoros Contínuos
  switch (currentRiskMode) {
    case SR_IDLE:
      break;

    case SR_FIRE_SIREN:
      // Não interrompe caso seja um teste manual ativo
      if (!riskSirensEnabled && !isManualFireTest && currentData.flame_detected == 0) {
        stopSiren();
        break;
      }
      if (now >= riskTimerNextStep) {
        riskTimerNextStep = now + 8;
        pwmTone3V(fireSirenFreq);

        if (fireSirenUp) {
          fireSirenFreq += 95;
          if (fireSirenFreq >= 3200) {
            fireSirenUp = false;
            fireStrobeState = !fireStrobeState;
            digitalTone5V(fireStrobeState);
          }
        } else {
          fireSirenFreq -= 95;
          if (fireSirenFreq <= 800) {
            fireSirenUp = true;
            fireStrobeState = !fireStrobeState;
            digitalTone5V(fireStrobeState);
          }
        }
      }
      break;

    case SR_GAS_ALARM:
      if (!riskSirensEnabled && !isManualGasTest) {
        stopGasAlarm();
        break;
      }
      if (now >= riskTimerNextStep) {
        gasPulseState = !gasPulseState;
        if (gasPulseState) {
          pwmTone3V(3100);
          digitalTone5V(true);
          riskTimerNextStep = now + 140;
        } else {
          pwmStop3V();
          digitalTone5V(false);
          riskTimerNextStep = now + 90;
        }
      }
      break;

    case SR_RAIN_MELODY:
      if (!riskSirensEnabled && !isManualRainTest) {
        stopRainMelody();
        break;
      }
      if (now >= riskTimerNextStep) {
        if (!jingleNotePlaying) {
          if (jingleNoteIndex >= totalChristmasNotes) {
            jingleNoteIndex = 0;
          }
          pwmTone3V(fullChristmasMelody[jingleNoteIndex]);
          jingleNotePlaying = true;
          riskTimerNextStep = now + fullChristmasDurations[jingleNoteIndex];
        } else {
          pwmStop3V();
          jingleNotePlaying = false;
          jingleNoteIndex++;
          riskTimerNextStep = now + 40;
        }
      }
      break;
  }
}