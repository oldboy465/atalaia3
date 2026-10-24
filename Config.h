#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_system.h>
#include <Wire.h>

// ============================================================================
// 1. PINAGEM FISICA DO HARDWARE (ESP32 XTENSA DUAL-CORE)
// ============================================================================
#define PIN_DHT            4   // GPIO 04 - DHT11 / DHT22 (Sensor Higrotérmico)
#define PIN_BUZZER_3V3    18   // GPIO 18 - Buzzer 3.3V (Sirenes, Melodias e Bips PWM)
#define PIN_LED_VERDE     19   // GPIO 19 - LED Verde Heartbeat (PWM LEDC Canal 0)
#define PIN_OLED_SDA      21   // GPIO 21 - Barramento I2C Dados OLED 0.96" (D21)
#define PIN_OLED_SCL      22   // GPIO 22 - Barramento I2C Clock OLED 0.96" (D22)
#define PIN_BUZZER_5V     27   // GPIO 27 - Buzzer 5V Ativo (Comando Digital Push-Pull)
#define PIN_RAIN_AO       32   // GPIO 32 - Sensor de Chuva Analógico (ADC1 CH4)
#define PIN_FLAME_DO      33   // GPIO 33 - Sensor IR de Chama Digital (Nível lógico configurável)
#define PIN_MQ135_AO      34   // GPIO 34 - Sensor de Gás MQ-135 Analógico (ADC1 CH6)

// ============================================================================
// 2. PARÂMETROS DO DISPLAY OLED SSD1306 (128x64 I2C BICOLOR)
// ============================================================================
#define SCREEN_WIDTH       128
#define SCREEN_HEIGHT      64
#define OLED_RESET         -1
#define OLED_I2C_ADDRESS   0x3C

#define OLED_YELLOW_HEIGHT        16     // Linhas 0 a 15: Banda física amarela (Barra de Status Celular)
#define OLED_BLUE_START_Y         16     // Linhas 16 a 63: Banda física azul (Terminal CMD)
#define OLED_BLUE_HEIGHT          48     // Altura da área útil do terminal azul

#define OLED_SCREEN_ROTATE_MS     4500UL // Intervalo de rotação das telas normais
#define OLED_POPUP_HOLD_MS        3800UL // Tempo de permanência do popup de tarefas
#define OLED_MATRIX_DURATION_MS   4500UL // Tempo da animação estilo Matrix ao conectar
#define OLED_TERMINAL_LINE_MAX    5      // Número máximo de linhas visíveis no terminal azul
#define OLED_TERMINAL_CHAR_LIMIT  21     // Limite de caracteres por linha horizontal (fonte 5x7)

// ============================================================================
// 3. TIMERS E CANAIS PWM DEDICADOS (LEDC HARDWARE TIMER)
// ============================================================================
#define LEDC_CH_LED        0
#define LEDC_CH_BUZZER3V   1

#define PWM_FREQ_LED       5000
#define PWM_RES_BITS       8

// ============================================================================
// 4. PARÂMETROS DE REDE, CAPTIVE PORTAL E SISTEMA DE ARQUIVOS LITTLEFS
// ============================================================================
extern const char* AP_SSID;
extern const char* AP_PASS;
extern const char* MDNS_HOSTNAME;
extern const byte  DNS_PORT;
extern const IPAddress AP_IP;
extern const IPAddress AP_NETMASK;

#define RING_BUFFER_SIZE         8640   // 12 horas a 5s por amostra física
#define RAM_BATCH_SIZE           12     // Lote de 12 amostras na RAM (1 minuto por flush físico)
#define FILE_BUFFER              "/atalaia_buffer.bin"
#define FILE_META                "/atalaia_meta.bin"
#define FILE_WIFI                "/wifi_config.json"
#define FILE_AUDIO               "/audio_config.json"
#define STORAGE_MAGIC_HEADER     0x41544133   // "ATA3" em hexadecimal
#define STORAGE_FORMAT_VERSION   3

// Limiares Operacionais e Temporização Sonora
#define RAIN_THRESHOLD           2400
#define DEFAULT_WARMUP_MS        300000UL
#define BEEP_TICK_MS             12   // Tick ultracurto, seco e atômico (12ms)

// Limites configuráveis de Gás (MQ-135 recalibrado para maior estabilidade)
#define GAS_PPM_MIN_ALLOWED      50.0f
#define GAS_PPM_MAX_ALLOWED      5000.0f
#define GAS_HYSTERESIS_DELTA     45.0f

// Parâmetros de Detecção Térmica Preditiva (dT/dt)
#define THERMAL_RATE_TRIGGER_C_MIN 2.2f   // Disparo se subir > 2.2 °C/minuto em janela curta
#define THERMAL_WINDOW_SAMPLES     12     // Janela de 60 segundos (12 amostras de 5s)

// NTP (Brasília UTC-3)
extern const char* NTP_SERVER_1;
extern const char* NTP_SERVER_2;
extern const long  GMT_OFFSET_SEC;
extern const int   DAYLIGHT_OFFSET_SEC;

// Credenciais Telegram
extern const char* TELEGRAM_BOT_TOKEN;
extern const int64_t AUTHORIZED_CHAT_ID;

// ============================================================================
// 5. TIPOS ENUMERADOS DO SISTEMA DE CONTROLE E DISPLAY
// ============================================================================
enum BeepDevice {
  BEEP_DEV_BUZZER_5V = 0,
  BEEP_DEV_BUZZER_3V = 1,
  BEEP_DEV_ALL       = 2
};

enum LedMode {
  LED_MODE_AUTO_HEARTBEAT = 0,
  LED_MODE_FORCE_ON       = 1,
  LED_MODE_FORCE_OFF      = 2
};

enum RiskLevel {
  RISK_NORMAL   = 0,
  RISK_ATENCAO  = 1,
  RISK_ALERTA   = 2,
  RISK_CRITICO  = 3
};

inline const char* getRiskLevelName(RiskLevel level) {
  switch (level) {
    case RISK_CRITICO: return "CRITICO";
    case RISK_ALERTA:   return "ALERTA";
    case RISK_ATENCAO:  return "ATENCAO";
    default:            return "NORMAL";
  }
}

enum DataQuality {
  DATA_VALID    = 0,
  DATA_SUSPECT  = 1,
  DATA_INVALID  = 2
};

enum MLState {
  ML_AMBIENTE_ESTAVEL          = 0,
  ML_CONDENSACAO_ORVALHO_CRIT  = 1,
  ML_ANOMALIA_PRE_COMBUSTAO    = 2,
  ML_CHUVA_CONVECTIVA          = 3
};

enum DisplayMode {
  DISP_MODE_CAROUSEL,
  DISP_MODE_MATRIX_RAIN,
  DISP_MODE_TASK_POPUP,
  DISP_MODE_HACKER_TERMINAL,
  DISP_MODE_PHONE_TERMINAL,
  DISP_MODE_FIRE_ALERT,
  DISP_MODE_CHRISTMAS_ANIM,
  DISP_MODE_CUSTOM_IMAGE
};

// ============================================================================
// 6. ESTRUTURAS DE DADOS BINÁRIAS E METADADOS
// ============================================================================
struct __attribute__((packed)) SensorData {
  float    temperature;         // Temperatura atual corrigida (°C)
  float    humidity;            // Umidade relativa corrigida (%)
  float    mq135_ppm_comp;      // Concentração de gás compensada por T/RH (PPM)
  float    heat_index;          // Sensação térmica calculada (°C)
  float    dew_point;           // Ponto de orvalho psicrométrico (°C)
  float    temp_rate_minute;    // Taxa de variação térmica derivada (dT/dt em °C/min)
  uint32_t uptime_sec;          // Uptime do ESP32 no momento da leitura (segundos)
  uint32_t session_id;          // Identificador único da sessão de boot (Random ID)
  int32_t  mq135_raw;           // ADC bruto do MQ-135 (0-4095)
  int32_t  rain_raw;            // ADC bruto do sensor de chuva (0-4095)
  uint8_t  rain_detected;       // 1 = chuva confirmada, 0 = seco
  uint8_t  flame_detected;      // 1 = foco de chama ativo via D33, 0 = normal
  uint8_t  risk_level;          // Nível da FSM multietapa (RiskLevel)
  uint8_t  data_quality;        // Validade das amostras ambientais (DataQuality)
  uint8_t  ml_state;            // Predição de borda pelo motor TinyML (MLState)
  uint8_t  checksum;            // Verificação de integridade XOR por registro
  uint8_t  padding[2];          // Alinhamento rígido em limites de 32 bits
};

struct __attribute__((packed)) StorageHeader {
  uint32_t magic;               // Assinatura STORAGE_MAGIC_HEADER
  uint16_t version;             // STORAGE_FORMAT_VERSION
  uint16_t record_size;         // sizeof(SensorData)
  uint32_t capacity;            // Capacidade máxima (RING_BUFFER_SIZE)
  uint32_t reserved;            // Expansão futura
};

struct __attribute__((packed)) RingMeta {
  uint32_t head_index;          // Ponteiro circular atual de escrita (0 a RING_BUFFER_SIZE - 1)
  uint32_t total_saved;         // Total acumulado vitalício de registros gravados
  uint32_t crc32;               // Checksum CRC32 dos metadados
};

struct AverageStats {
  float avgTemp;
  float avgHum;
  float avgMq;
  float minTemp;
  float maxTemp;
  float minHum;
  float maxHum;
  int   count;
};

#define MAX_EVENT_LOGS 30
struct EventLogEntry {
  uint32_t uptimeSec;
  String   timeStr;
  String   category;
  String   message;
  uint8_t  severity;            // 0: INFO, 1: WARN, 2: CRITICAL
};

// ============================================================================
// 7. DECLARAÇÕES DE ESTADO GLOBAL E DISPOSITIVOS VIRTUAIS
// ============================================================================
extern WebServer server;
extern DNSServer dnsServer;

extern SensorData currentData;
extern RingMeta   ringMeta;
extern uint32_t   currentSessionId;

extern EventLogEntry eventLogs[MAX_EVENT_LOGS];
extern int eventLogHead;
extern int eventLogCount;

extern String locationInfo;
extern String locationCityRegion;
extern String locationLatitude;
extern String locationLongitude;

extern unsigned long lastGeoLookup;
extern bool ntpSynced;
extern unsigned long lastNtpSyncAttempt;

extern bool isSystemStandby;
extern bool masterSoundEnabled;
extern bool collectionBeepEnabled;
extern bool riskSirensEnabled;
extern bool collectionFlashEnabled;
extern LedMode currentLedMode;
extern BeepDevice selectedBeepDevice;
extern float gasAlertThresholdPpm;

extern unsigned long warmupDurationMs;

extern bool isApMode;
extern String savedSsid;
extern String savedPass;
extern bool pendingWifiReconnect;
extern unsigned long wifiReconnectTimer;
extern unsigned long lastWifiRetry;
extern unsigned long lastApStaCheck;

extern unsigned long lastTelegramPoll;
extern unsigned long lastHourlyReport;
extern int64_t lastTelegramUpdateId;

extern bool lastFlameNotified;
extern bool lastRainNotified;
extern bool lastGasNotified;
extern bool lastPreFireNotified;

extern unsigned long flameAlertCooldown;
extern unsigned long rainAlertCooldown;
extern unsigned long gasAlertCooldown;
extern unsigned long preFireAlertCooldown;

extern uint32_t telegramSentCount;
extern uint32_t telegramFailCount;

extern unsigned long lastSampleTime;

extern bool isDisplayPinnedState;
extern bool showIpOnDisplay;
extern bool flameActiveLow;

extern uint8_t carouselPageIndex;
extern const uint8_t TOTAL_CAROUSEL_PAGES;

#endif // CONFIG_H