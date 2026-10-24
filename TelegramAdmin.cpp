#include "TelegramAdmin.h"
#include "TelegramBot.h"
#include "Storage.h"
#include <WiFiClientSecure.h>

bool validateSenderAuthorization(int64_t chatId) {
  return (chatId == AUTHORIZED_CHAT_ID);
}

bool resetTelegramWebhook(bool dropUpdates) {
  if (WiFi.status() != WL_CONNECTED || WiFi.localIP()[0] == 0) return false;

  if (ESP.getFreeHeap() < 20000) {
    Serial.printf("[TELEGRAM-ADMIN] Heap insuficiente: Free=%u\n", ESP.getFreeHeap());
    return false;
  }

  if (!acquireTelegramLock(pdMS_TO_TICKS(6000))) return false;

  bool success = false;
  {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(6000);

    if (client.connect("api.telegram.org", 443)) {
      String url = "/bot" + String(TELEGRAM_BOT_TOKEN) + "/deleteWebhook";
      if (dropUpdates) {
        url += "?drop_pending_updates=true";
      } else {
        url += "?drop_pending_updates=false";
      }

      client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                   "Host: api.telegram.org\r\n" +
                   "User-Agent: ESP32-ATALAIA3\r\n" +
                   "Connection: close\r\n\r\n");

      unsigned long start = millis();
      String response = "";
      response.reserve(256);

      while (client.connected() && (millis() - start < 4000)) {
        while (client.available()) {
          char c = (char)client.read();
          response += c;
        }
        if (response.indexOf("\r\n\r\n") != -1 && response.indexOf("}") != -1) break;
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      client.stop();

      if (response.indexOf("\"ok\":true") != -1) {
        Serial.println("[TELEGRAM-ADMIN] Webhook desvinculado com sucesso.");
        success = true;
      }
    }
  }

  releaseTelegramLock();
  return success;
}

bool testTelegramApiConnection() {
  if (WiFi.status() != WL_CONNECTED || WiFi.localIP()[0] == 0) return false;

  if (ESP.getFreeHeap() < 20000) return false;

  if (!acquireTelegramLock(pdMS_TO_TICKS(6000))) return false;

  bool success = false;
  {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(6000);

    if (client.connect("api.telegram.org", 443)) {
      String url = "/bot" + String(TELEGRAM_BOT_TOKEN) + "/getMe";
      client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                   "Host: api.telegram.org\r\n" +
                   "User-Agent: ESP32-ATALAIA3\r\n" +
                   "Connection: close\r\n\r\n");

      unsigned long start = millis();
      String response = "";
      response.reserve(256);

      while (client.connected() && (millis() - start < 4000)) {
        while (client.available()) {
          char c = (char)client.read();
          response += c;
        }
        if (response.indexOf("\r\n\r\n") != -1 && response.indexOf("}") != -1) break;
        vTaskDelay(pdMS_TO_TICKS(10));
      }
      client.stop();

      if (response.indexOf("\"ok\":true") != -1) {
        int userIdx = response.indexOf("\"username\":\"");
        String botUsername = "Bot Ativo";
        if (userIdx != -1) {
          int endUser = response.indexOf("\"", userIdx + 12);
          if (endUser != -1) {
            botUsername = "@" + response.substring(userIdx + 12, endUser);
          }
        }
        Serial.printf("[TELEGRAM-DIAG] Autenticado como %s\n", botUsername.c_str());
        success = true;
      }
    }
  }

  releaseTelegramLock();
  return success;
}

bool purgeTelegramPendingUpdates() {
  return resetTelegramWebhook(true);
}