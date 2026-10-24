#include "NetworkManager.h"
#include "Storage.h"
#include "Diagnostics.h"
#include "DisplayManager.h"
#include "TelegramBot.h"
#include <esp_wifi.h>
#include <ESPmDNS.h>

const char* AP_SSID       = "atalaia3";
const char* AP_PASS       = "sua_senha_aqui";
const char* MDNS_HOSTNAME = "atalaia3";
const byte  DNS_PORT      = 53;
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_NETMASK(255, 255, 255, 0);

const char* NTP_SERVER_1        = "a.st1.ntp.br";
const char* NTP_SERVER_2        = "pool.ntp.org";
const long  GMT_OFFSET_SEC      = -10800;
const int   DAYLIGHT_OFFSET_SEC = 0;

static unsigned long lastConnectionAttempt = 0;
static uint8_t connectionRetryBackoffSec = 5;

void startAccessPoint() {
  Serial.println("\n[REDE] =========================================");
  Serial.println("[REDE] INICIALIZANDO SOFT-AP 'atalaia3' SEGURO...");
  Serial.println("[REDE] =========================================");

  WiFi.disconnect(true, true);
  delay(120);

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.setSleep(false);
  WiFi.softAPConfig(AP_IP, AP_IP, AP_NETMASK);

  bool apOk = WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 4);
  if (apOk) {
    isApMode = true;
    locationInfo = "(SEM LOCAL - MODO AP)";
    Serial.printf("[REDE] AP ATIVO COM SUCESSO!\n");
    Serial.printf("[REDE] SSID: %s | IP: %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("[REDE] FALHA CRITICA AO INICIAR WiFi.softAP!");
  }

  dnsServer.stop();
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", AP_IP);

  MDNS.end();
  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
  }

  addEventLog("REDE", "Ponto de Acesso 'atalaia3' operacional em 192.168.4.1", 0);
  triggerDisplayTaskPopup("Modo SoftAP", "SSID: atalaia3", 3000);
  appendTerminalCmdLine("[REDE] SoftAP Ativado");
}

bool connectStation(const String& ssid, const String& pass) {
  if (ssid.length() == 0) return false;

  Serial.printf("\n[WIFI] Tentando associar ao ponto de acesso: %s\n", ssid.c_str());

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), pass.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 25) {
    delay(240);
    Serial.print(".");
    attempts++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    isApMode = false;
    dnsServer.stop();
    WiFi.mode(WIFI_STA);

    MDNS.end();
    if (MDNS.begin(MDNS_HOSTNAME)) {
      MDNS.addService("http", "tcp", 80);
    }

    connectionRetryBackoffSec = 5;
    Serial.printf("[WIFI] Conexao estabelecida! IP Obtido: %s\n", WiFi.localIP().toString().c_str());
    addEventLog("REDE", "Conectado ao Wi-Fi: " + ssid + " (IP: " + WiFi.localIP().toString() + ")", 0);

    triggerDisplayMatrixAnimation(4500);
    appendTerminalCmdLine("[WIFI] Conectado STA");
    appendTerminalCmdLine("IP: " + WiFi.localIP().toString());

    syncNtpTime();
    fetchGeoLocation();

    // Inicialização direta da pilha TLS com buffers de 2 kB
    initTelegramBot();
    return true;
  }

  Serial.println("[WIFI] Nao foi possivel associar. Restaurando SoftAP de recuperacao...");
  startAccessPoint();
  return false;
}

void setupNetworkStack() {
  loadWifiCredentials();
  if (savedSsid.length() > 0) {
    Serial.printf("[REDE] Credenciais encontradas na Flash para: %s\n", savedSsid.c_str());
    if (!connectStation(savedSsid, savedPass)) {
      addEventLog("REDE", "Roteador salvo inacessivel. Subindo SoftAP.", 1);
      startAccessPoint();
    }
  } else {
    Serial.println("[REDE] Sem credenciais salvas. Inicializando SoftAP de configuracao...");
    startAccessPoint();
  }
}

bool isStationConnected() {
  return (!isApMode && WiFi.status() == WL_CONNECTED && WiFi.localIP()[0] != 0);
}

int8_t getCurrentRssi() {
  if (isStationConnected()) {
    return WiFi.RSSI();
  }
  return 0;
}

void handleNetworkMaintenance() {
  unsigned long now = millis();

  if (isApMode) {
    dnsServer.processNextRequest();
  }

  if (pendingWifiReconnect && now >= wifiReconnectTimer) {
    pendingWifiReconnect = false;
    connectStation(savedSsid, savedPass);
  }

  if (!isApMode) {
    if (WiFi.status() == WL_CONNECTED) {
      if (locationInfo.startsWith("(SEM LOCAL") && (now - lastGeoLookup >= 30000UL)) {
        lastGeoLookup = now;
        fetchGeoLocation();
      }
      if (!ntpSynced && (now - lastNtpSyncAttempt >= 60000UL)) {
        lastNtpSyncAttempt = now;
        syncNtpTime();
      }
    } else {
      locationInfo = "(SEM LOCAL - SEM SINAL)";
      if (now - lastConnectionAttempt >= (connectionRetryBackoffSec * 1000UL)) {
        lastConnectionAttempt = now;
        Serial.printf("[WIFI] Sinal desconectado. Tentativa de reconexao progressiva (Espera: %us)...\n", connectionRetryBackoffSec);
        WiFi.reconnect();

        if (connectionRetryBackoffSec < 60) {
          connectionRetryBackoffSec += 10;
        }
      }
    }
  }
}