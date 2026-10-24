#include "Diagnostics.h"
#include "Storage.h"
#include <esp_system.h>

#ifdef __cplusplus
extern "C" {
#endif
// Declaração da rotina nativa da ROM/ESP-IDF para leitura do sensor térmico do silício
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif

static esp_reset_reason_t bootResetReason = ESP_RST_UNKNOWN;

void initDiagnostics() {
  bootResetReason = esp_reset_reason();
  String reasonStr = getResetReasonDescription(bootResetReason);
  
  Serial.println("[DIAG] =========================================");
  Serial.printf("[DIAG] Causa do Último Reset: %s (Código %d)\n", reasonStr.c_str(), (int)bootResetReason);
  Serial.printf("[DIAG] Frequência dos Núcleos: %u MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("[DIAG] Memória Heap Inicial : %u Bytes\n", ESP.getFreeHeap());
  Serial.println("[DIAG] =========================================");

  addEventLog("BOOT_DIAG", "Boot verificado: " + reasonStr, (bootResetReason == ESP_RST_PANIC || bootResetReason == ESP_RST_TASK_WDT) ? 2 : 0);
}

String getResetReasonDescription(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:
      return "Power-on / Energizacao a Frio";
    case ESP_RST_EXT:
      return "Reset Externo via Pino EN";
    case ESP_RST_SW:
      return "Reinicio por Software (esp_restart)";
    case ESP_RST_PANIC:
      return "Falha Critica / Excecao de Kernel (Panic)";
    case ESP_RST_INT_WDT:
      return "Watchdog de Interrupcao (IWDT)";
    case ESP_RST_TASK_WDT:
      return "Watchdog de Tarefa Bloqueada (TWDT)";
    case ESP_RST_WDT:
      return "Watchdog Geral de Hardware";
    case ESP_RST_DEEPSLEEP:
      return "Retorno de Suspensao Deep-Sleep";
    case ESP_RST_BROWNOUT:
      return "Subtensao de Alimentacao (Brownout)";
    case ESP_RST_SDIO:
      return "Reinicio via Barramento SDIO";
    default:
      return "Causa Nao Determinada / Desconhecida";
  }
}

uint8_t calculateSignalQuality(int8_t rssi) {
  if (rssi <= -100 || rssi >= 0) return 0;
  if (rssi >= -50) return 100;
  // Mapeamento linear padrão de telecomunicações entre -100 dBm e -50 dBm
  return (uint8_t)(2 * (rssi + 100));
}

SystemHealth getSystemHealth() {
  SystemHealth health;

  health.freeHeap     = ESP.getFreeHeap();
  health.minFreeHeap  = ESP.getMinFreeHeap();
  health.maxAllocHeap = ESP.getMaxAllocHeap();

  // Cálculo da taxa de fragmentação de memória Heap:
  // Se maxAllocHeap for substancialmente menor que freeHeap, existe severa fragmentação
  if (health.freeHeap > 0) {
    float ratio = (float)health.maxAllocHeap / (float)health.freeHeap;
    if (ratio > 1.0f) ratio = 1.0f;
    health.heapFragmentation = (uint8_t)((1.0f - ratio) * 100.0f);
  } else {
    health.heapFragmentation = 100;
  }

  // Leitura da temperatura interna do SoC ESP32
#if defined(ESP_ARDUINO_VERSION_VAL)
  #if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    health.cpuTemperature = temperatureRead();
  #else
    // Conversão da leitura bruta analógica interna da ESP-IDF (em Fahrenheit) para Celsius
    health.cpuTemperature = (temprature_sens_read() - 32.0f) / 1.8f;
  #endif
#else
  health.cpuTemperature = (temprature_sens_read() - 32.0f) / 1.8f;
#endif

  health.uptimeSeconds = millis() / 1000UL;
  health.cpuFreqMHz    = ESP.getCpuFreqMHz();
  health.wifiRssi      = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
  health.wifiSignalQuality = calculateSignalQuality(health.wifiRssi);
  health.resetCode     = bootResetReason;
  health.resetReason   = getResetReasonDescription(bootResetReason);

  return health;
}