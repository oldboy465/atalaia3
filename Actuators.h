#ifndef ACTUATORS_H
#define ACTUATORS_H

#include "Config.h"

// ============================================================================
// MÁQUINA DE ESTADOS DOS MODOS DE ÁUDIO E RISCO SONORO
// ============================================================================
enum SoundRiskMode {
  SR_IDLE,
  SR_FIRE_SIREN,
  SR_GAS_ALARM,
  SR_RAIN_MELODY
};

// Sinalizadores de teste manual acessíveis para garantir não-interrupção
extern bool isManualFireTest;
extern bool isManualGasTest;
extern bool isManualRainTest;

// ============================================================================
// INICIALIZAÇÃO E CONTROLE DE BAIXO NÍVEL DOS CANAIS PWM LEDC E GPIO
// ============================================================================
void pwmInit();
void pwmWriteLed(uint32_t duty);

void pwmTone3V(uint32_t freq);
void pwmStop3V();
void digitalTone5V(bool state);

// ============================================================================
// FEEDBACK ATÔMICO DE COLETA E TESTES DE BANCA (PRESERVAÇÃO INTEGRAL)
// ============================================================================
void triggerCollectionFeedback(unsigned long beepMs = BEEP_TICK_MS);
void triggerManualBeepTest(BeepDevice targetDevice, unsigned long beepMs = 60);

// ============================================================================
// GESTÃO NÃO-BLOQUEANTE DE SIRENES E MELODIAS POLIFÔNICAS
// ============================================================================
void startSiren(bool isManualTest = false, unsigned long manualDurationMs = 4000);
void stopSiren();

void startGasAlarm(bool isManualTest = false, unsigned long manualDurationMs = 4000);
void stopGasAlarm();

void startRainMelody(bool isManualTest = false);
void stopRainMelody();

void playFullChristmasMelody();

void stopAllSounds();
void updateBuzzersAndLed();
void setLedMode(LedMode mode);

#endif // ACTUATORS_H