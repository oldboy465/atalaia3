#include "Config.h"
#include "Actuators.h"
#include "Sensors.h"
#include "Storage.h"
#include "Diagnostics.h"
#include "NetworkManager.h"
#include "WebInterface.h"
#include "TelegramBot.h"
#include "TelegramAdmin.h"
#include "DisplayManager.h"
#include "images.h"
#include <esp_random.h>

// ============================================================================
// DEFINIÇÃO E ALOCAÇÃO DAS VARIÁVEIS GLOBAIS DO SISTEMA
// ============================================================================
WebServer server(80);
DNSServer dnsServer;

SensorData currentData;
RingMeta   ringMeta;
uint32_t   currentSessionId = 0;

EventLogEntry eventLogs[MAX_EVENT_LOGS];
int eventLogHead  = 0;
int eventLogCount = 0;

String locationInfo = "(SEM LOCAL)";
unsigned long lastGeoLookup = 0;
bool ntpSynced = false;
unsigned long lastNtpSyncAttempt = 0;

bool isSystemStandby        = false;
bool masterSoundEnabled     = true;
bool collectionBeepEnabled  = true;
bool riskSirensEnabled      = true;
bool collectionFlashEnabled = true;
LedMode currentLedMode      = LED_MODE_AUTO_HEARTBEAT;
BeepDevice selectedBeepDevice = BEEP_DEV_BUZZER_5V;
float gasAlertThresholdPpm  = 600.0f;

unsigned long warmupDurationMs = DEFAULT_WARMUP_MS;

bool isApMode               = true;
String savedSsid            = "";
String savedPass            = "";
bool pendingWifiReconnect   = false;
unsigned long wifiReconnectTimer = 0;
unsigned long lastWifiRetry      = 0;
unsigned long lastApStaCheck     = 0;

unsigned long lastTelegramPoll  = 0;
unsigned long lastHourlyReport  = 0;
int64_t lastTelegramUpdateId    = 0;

bool lastFlameNotified   = false;
bool lastRainNotified    = false;
bool lastGasNotified     = false;
bool lastPreFireNotified = false;

static bool fireAlarmTriggered = false;

unsigned long flameAlertCooldown   = 0;
unsigned long rainAlertCooldown    = 0;
unsigned long gasAlertCooldown     = 0;
unsigned long preFireAlertCooldown = 0;

uint32_t telegramSentCount = 0;
uint32_t telegramFailCount = 0;

unsigned long lastSampleTime = 0;

// Credenciais Mestras Homologadas
const char* TELEGRAM_BOT_TOKEN   = "SEU_TELEGRAM_BOT_TOKEN_AQUI";
const int64_t AUTHORIZED_CHAT_ID =  0000000000;

TaskHandle_t networkTaskHandle = NULL;

// Processamento Exaustivo de Comandos no Console Serial (Baud 115200)
void processSerialCommands() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  String cmdLower = line;
  cmdLower.toLowerCase();

  if (cmdLower == "status" || cmdLower == "report") {
    triggerDisplayTaskPopup("Serial CMD", "Relatorio Status", 2500);
    Serial.println(buildReportMessage());
  } else if (cmdLower == "on" || cmdLower == "ligar") {
    setSystemStandby(false, true);
    triggerDisplayTaskPopup("Energia", "MODO OPERACIONAL", 2500);
    Serial.println("[CMD] Sistema colocado em modo Operacional.");
  } else if (cmdLower == "off" || cmdLower == "desligar" || cmdLower == "standby") {
    setSystemStandby(true, true);
    triggerDisplayTaskPopup("Energia", "MODO STANDBY", 2500);
    Serial.println("[CMD] Sistema pausado em Standby.");
  } else if (cmdLower == "beep") {
    triggerManualBeepTest(selectedBeepDevice, 30);
    triggerDisplayTaskPopup("Bip Bancada", "Atuador Padrao", 2000);
    Serial.println("[CMD] Bip padrão executado.");
  } else if (cmdLower == "beep5v") {
    triggerManualBeepTest(BEEP_DEV_BUZZER_5V, 40);
    triggerDisplayTaskPopup("Bip Bancada", "Buzzer 5V (D27)", 2000);
    Serial.println("[CMD] Bip curto Buzzer 5V (GPIO 27) executado.");
  } else if (cmdLower == "beep3v") {
    triggerManualBeepTest(BEEP_DEV_BUZZER_3V, 40);
    triggerDisplayTaskPopup("Bip Bancada", "Buzzer 3.3V (D18)", 2000);
    Serial.println("[CMD] Bip curto Buzzer 3.3V (GPIO 18) executado.");
  } else if (cmdLower == "beepall") {
    triggerManualBeepTest(BEEP_DEV_ALL, 45);
    triggerDisplayTaskPopup("Bip Bancada", "AMBOS (D18 + D27)", 2000);
    Serial.println("[CMD] Bip duplo (D18 + D27) executado.");
  } else if (cmdLower == "siren" || cmdLower == "fire") {
    startSiren(true, 3500);
    triggerDisplayTaskPopup("Sirene Fogo", "Varredura 3.5s", 3500);
    Serial.println("[CMD] Sirene de Incêndio Louca acionada (3.5s).");
  } else if (cmdLower == "gas") {
    startGasAlarm(true, 3500);
    triggerDisplayTaskPopup("Alarme Gas", "Pulsos 3.1kHz", 3500);
    Serial.println("[CMD] Alarme de Gás acionado (3.5s).");
  } else if (cmdLower == "rain" || cmdLower == "natal") {
    triggerDisplayTaskPopup("Melodia", "Jingle Bells", 4000);
    Serial.println("[CMD] Executando partitura natalícia e animação no OLED...");
    startRainMelody(true);
  } else if (cmdLower == "stop") {
    stopAllSounds();
    stopDisplayCustomImage();
    triggerDisplayTaskPopup("Audio", "SILENCIADO", 2000);
    Serial.println("[CMD] Todos os atuadores sonoros foram silenciados.");
  } else if (cmdLower == "ap") {
    triggerDisplayTaskPopup("Rede", "Comutando SoftAP", 2500);
    Serial.println("[CMD] Comutando para Ponto de Acesso Seguro (atalaia3)...");
    startAccessPoint();
  } else if (cmdLower == "sta") {
    triggerDisplayTaskPopup("Rede", "Reconectando STA", 2500);
    Serial.println("[CMD] Reconectando ao roteador Station...");
    setupNetworkStack();
  } else if (cmdLower == "flush") {
    flushRamBatch();
    triggerDisplayTaskPopup("Flash FS", "Flush Lote RAM", 2000);
    Serial.println("[CMD] Lote de RAM descarregado na partição LittleFS.");
  } else if (cmdLower == "matrix") {
    Serial.println("[CMD] Animação Matrix acionada no Display OLED.");
    triggerDisplayMatrixAnimation(4500);
  } else if (cmdLower == "next" || cmdLower == "proxima") {
    nextDisplayScreen();
    triggerDisplayTaskPopup("Display", "Proxima Tela", 1500);
    Serial.printf("[CMD] Tela OLED alterada para índice %u (%s).\n", getCurrentDisplayScreen(), getDisplayScreenName(getCurrentDisplayScreen()));
  } else if (cmdLower == "prev" || cmdLower == "anterior") {
    previousDisplayScreen();
    triggerDisplayTaskPopup("Display", "Tela Anterior", 1500);
    Serial.printf("[CMD] Tela OLED alterada para índice %u (%s).\n", getCurrentDisplayScreen(), getDisplayScreenName(getCurrentDisplayScreen()));
  } else if (cmdLower.startsWith("screen ") || cmdLower.startsWith("tela ")) {
    int scIdx = cmdLower.substring(5).toInt() - 1;
    if (scIdx >= 0 && scIdx < getTotalDisplayScreens()) {
      selectDisplayScreen((uint8_t)scIdx);
      triggerDisplayTaskPopup("Display", getDisplayScreenName((uint8_t)scIdx), 1500);
      Serial.printf("[CMD] Tela OLED comutada diretamente para tela %d: %s\n", scIdx + 1, getDisplayScreenName((uint8_t)scIdx));
    } else {
      Serial.printf("[CMD] Índice de tela inválido! Informe de 1 a %u.\n", getTotalDisplayScreens());
    }
  } else if (cmdLower == "pin" || cmdLower == "fixar") {
    setDisplayPinned(true);
    triggerDisplayTaskPopup("Display", "TELA FIXADA", 2000);
    Serial.println("[CMD] Tela Celular / Terminal CMD FIXADA no OLED.");
  } else if (cmdLower == "unpin" || cmdLower == "soltar") {
    setDisplayPinned(false);
    triggerDisplayTaskPopup("Display", "CARROSSEL ATIVO", 2000);
    Serial.println("[CMD] Fixacao desfeita. Carrossel OLED liberado.");
  } else if (cmdLower == "ip display on") {
    showIpOnDisplay = true;
    saveAudioAndPowerConfig();
    Serial.println("[CMD] Exibição do IP no display ativada.");
  } else if (cmdLower == "ip display off") {
    showIpOnDisplay = false;
    saveAudioAndPowerConfig();
    Serial.println("[CMD] Exibição do IP no display desativada.");
  } else if (cmdLower == "flame pol" || cmdLower == "fogo pol") {
    flameActiveLow = !flameActiveLow;
    saveAudioAndPowerConfig();
    Serial.printf("[CMD] Polaridade do sensor de fogo (D33): %s\n", flameActiveLow ? "LOW Ativo" : "HIGH Ativo");
  } else if (cmdLower.startsWith("img ")) {
    int imgNum = cmdLower.substring(4).toInt() - 1;
    if (imgNum >= 0 && imgNum < SYSTEM_IMAGE_COUNT) {
      triggerDisplayCustomImage((uint8_t)imgNum, 5000);
      Serial.printf("[CMD] Imagem %d (%s) projetada por 5 segundos no OLED.\n", imgNum + 1, SYSTEM_IMAGES[imgNum].name);
    } else {
      Serial.printf("[CMD] Índice de imagem inválido! Existem %u imagens disponíveis.\n", SYSTEM_IMAGE_COUNT);
    }
  } else if (cmdLower == "stop img") {
    stopDisplayCustomImage();
    Serial.println("[CMD] Projeção de imagem interrompida.");
  } else if (cmdLower.startsWith("msg ")) {
    String m = line.substring(4);
    m.trim();
    pushTelegramMessageToDisplay("SER", m);
    Serial.printf("[CMD] Mensagem enviada para a tela do OLED: '%s'\n", m.c_str());
  } else if (cmdLower.startsWith("cmd ")) {
    String msg = line.substring(4);
    msg.trim();
    triggerDisplayHackerTerminal(msg);
    Serial.printf("[CMD] Terminal Hacker OLED acionado com a mensagem: '%s'\n", msg.c_str());
  } else if (cmdLower == "tgtest") {
    Serial.println("[CMD] Testando conexao direta com API Telegram...");
    testTelegramApiConnection();
  } else if (cmdLower == "tgreset") {
    Serial.println("[CMD] Resetando webhook ativo do Telegram...");
    resetTelegramWebhook(true);
  } else if (cmdLower == "reboot") {
    flushRamBatch();
    saveMetadataNow();
    triggerDisplayTaskPopup("Reboot", "Reiniciando ESP32", 1500);
    Serial.println("[CMD] A reiniciar ESP32...");
    delay(250);
    ESP.restart();
  } else if (cmdLower == "help" || cmdLower == "?") {
    Serial.println("\n╔══════════════════════════════════════════════════════════╗");
    Serial.println("║          ATALAIA 3 — CONSOLE DE DESENVOLVIMENTO         ║");
    Serial.println("╚══════════════════════════════════════════════════════════╝");
    Serial.println("• status | report        : Exibe relatório de telemetria completo");
    Serial.println("• on | off               : Comuta estação entre Operacional e Standby");
    Serial.println("• next | prev            : Navega entre as telas do OLED");
    Serial.println("• tela <1..4>            : Comuta diretamente para uma tela específica");
    Serial.println("• pin | unpin            : Fixa ou libera o carrossel no display");
    Serial.println("• ip display on|off      : Ativa ou oculta o endereço IP no display");
    Serial.println("• img <1..N>             : Exibe imagem personalizada sem travar CPU");
    Serial.println("• stop img               : Cancela a projeção de imagem ativa");
    Serial.println("• flame pol              : Inverte nível ativo do sensor de fogo (LOW/HIGH)");
    Serial.println("• beep | beep5v | beep3v : Testes de bancada acústicos");
    Serial.println("• siren | fire           : Teste da sirene de incêndio contínua (3.5s)");
    Serial.println("• gas                    : Teste sonoro do alarme de gás (3.5s)");
    Serial.println("• rain | natal           : Executa partitura natalícia e animação OLED");
    Serial.println("• stop                   : Silencia instantaneamente qualquer emissor");
    Serial.println("• ap | sta               : Alterna interface entre SoftAP e Station");
    Serial.println("• flush                  : Força descarga do lote de RAM na Flash");
    Serial.println("• matrix                 : Dispara chuva de caracteres estilo Matrix");
    Serial.println("• msg <texto>            : Envia mensagem para o terminal CMD azul");
    Serial.println("• cmd <mensagem>         : Escreve mensagem estilo hacker no display");
    Serial.println("• tgtest | tgreset       : Diagnóstico e desbloqueio do bot Telegram");
    Serial.println("• reboot                 : Grava metadados e reinicia a CPU\n");
  } else {
    Serial.printf("[CMD] Comando desconhecido: '%s'. Digite 'help' para a lista de comandos.\n", line.c_str());
  }
}

// Tarefa dedicada de rede FreeRTOS (Core 0)
void networkTask(void *pvParameters) {
  Serial.printf("[FREERTOS] NetworkTask iniciada com sucesso no Núcleo %d\n", xPortGetCoreID());

  for (;;) {
    handleNetworkMaintenance();
    server.handleClient();

    unsigned long now = millis();

    // Sondagem de comandos Telegram a cada 2500ms
    if (!isApMode && isStationConnected()) {
      if (now - lastTelegramPoll >= 2500UL) {
        lastTelegramPoll = now;
        pollTelegramCommands();
      }

      // Relatório horário automático (a cada 60 min fora do standby)
      if (!isSystemStandby && (now - lastHourlyReport >= 3600000UL)) {
        lastHourlyReport = now;
        sendTelegramMessage(
          "🕐 <b>RELATÓRIO HORÁRIO AUTOMÁTICO — ATALAIA 3</b>\n" + buildReportMessage(),
          false);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println("\n\n========================================================");
  Serial.println("   ATALAIA 3 — ESTACAO DE MONITORAMENTO AMBIENTAL");
  Serial.println("   Dual-Core baremetal C++ | LittleFS | TinyML | OLED 0.96");
  Serial.println("========================================================");

  currentSessionId = esp_random();

  initDiagnostics();
  initStorageSystem();
  loadMetadata();
  loadAudioAndPowerConfig();
  pwmInit();
  initSensors();
  initDisplay();

  setupNetworkStack();
  setupWebServer();

  // Alocação de 8192 bytes no Core 0
  xTaskCreatePinnedToCore(
    networkTask,
    "NetworkTask",
    8192,
    NULL,
    1,
    &networkTaskHandle,
    0
  );

  lastSampleTime = millis();
  addEventLog("BOOT", "Estacao ATALAIA 3 pronta para monitoramento.", 0);
  triggerManualBeepTest(selectedBeepDevice, 45);
}

void loop() {
  unsigned long now = millis();

  // 1. Monitoramento ultra-rápido de chama óptica no Pino D33
  bool flameNow = updateFlameStatusNonBlocking();
  currentData.flame_detected = flameNow ? 1 : 0;

  if (flameNow) {
    if (!fireAlarmTriggered) {
      startSiren(false, 0);
      fireAlarmTriggered = true;
    }
    if (currentLedMode == LED_MODE_AUTO_HEARTBEAT) {
      pwmWriteLed(255);
    }
    if (!lastFlameNotified || (now - flameAlertCooldown > 15000UL)) {
      addEventLog("EMERGENCIA", "Foco de chama detectado no pino D33!", 2);
      sendTelegramMessage(
        "🚨🚨🚨 <b>ALERTA CRÍTICO DE INCÊNDIO — ATALAIA 3</b> 🚨🚨🚨\n\n"
        "• Sensor Óptico IR (D33): <b>FOGO DETECTADO!</b>\n"
        "• Polaridade Configurada: <b>" + String(flameActiveLow ? "LOW Ativo" : "HIGH Ativo") + "</b>\n"
        "• Temperatura: " + String(currentData.temperature, 1) + " °C\n"
        "• Umidade: " + String(currentData.humidity, 1) + " %\n"
        "• Leitura MQ: ADC " + String(currentData.mq135_raw) + " (~" + String((int)currentData.mq135_ppm_comp) + " ppm)\n"
        "• Local: " + locationInfo + "\n"
        "• Coordenadas: " + locationLatitude + ", " + locationLongitude + "\n"
        "• Sirene contínua e atuadores ativados em alta prioridade!",
        true);
      lastFlameNotified = true;
      flameAlertCooldown = now;
    }
  } else {
    lastFlameNotified = false;
    if (fireAlarmTriggered) {
      stopSiren();
      fireAlarmTriggered = false;
    }
  }

  // 2. FSM de áudio e iluminação (Buzzers D18/D27 e LED D19)
  updateBuzzersAndLed();

  // 3. Renderização contínua e assíncrona do Display OLED Bicolor (Amarelo/Azul)
  updateDisplayNonBlocking();

  // 4. Aquisição periódica dos sensores ambientais a cada 5 segundos
  if (!isSystemStandby && (now - lastSampleTime >= 5000UL)) {
    lastSampleTime = now;
    executePeriodicSampling();
  }

  // 5. Atendimento a comandos manuais via Monitor Serial
  processSerialCommands();

  yield();
}