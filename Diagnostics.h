#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include "Config.h"
#include <esp_system.h>

// ============================================================================
// ESTRUTURA DE TELEMETRIA E SAÚDE DO HARDWARE ESP32
// ============================================================================
struct SystemHealth {
  uint32_t freeHeap;            // Memória Heap livre instantânea (Bytes)
  uint32_t minFreeHeap;         // Menor marca d'água histórica de Heap (Bytes)
  uint32_t maxAllocHeap;        // Maior bloco contíguo alocável na Heap (Bytes)
  uint8_t  heapFragmentation;   // Estimativa de fragmentação da Heap (0% a 100%)
  float    cpuTemperature;      // Temperatura do silício do SoC ESP32 (°C)
  uint32_t uptimeSeconds;       // Tempo contínuo de atividade do processador (s)
  uint32_t cpuFreqMHz;          // Frequência de operação dos núcleos Xtensa (MHz)
  int8_t   wifiRssi;            // Nível de sinal do rádio Wi-Fi (dBm)
  uint8_t  wifiSignalQuality;   // Qualidade percentual do sinal (0% a 100%)
  esp_reset_reason_t resetCode; // Código interno da última reinicialização
  String   resetReason;         // Causa legível do último reset físico/lógico
};

// ============================================================================
// PROTÓTIPOS DAS FUNÇÕES DE DIAGNÓSTICO E TELEMETRIA
// ============================================================================
void initDiagnostics();
SystemHealth getSystemHealth();
String getResetReasonDescription(esp_reset_reason_t reason);
uint8_t calculateSignalQuality(int8_t rssi);

#endif // DIAGNOSTICS_H