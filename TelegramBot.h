#ifndef TELEGRAM_BOT_H
#define TELEGRAM_BOT_H

#include "Config.h"
#include <WiFiClientSecure.h>

// Sincronização e Bloqueio Global TLS Inter-Core
bool acquireTelegramLock(TickType_t waitTicks = pdMS_TO_TICKS(8000));
void releaseTelegramLock();

// Transmissão Segura de Dados e Documentos
bool sendTelegramMessage(const String& text, bool priorityAlarm = false);
bool sendTelegramDocument(const String& filePath, const String& caption);
bool sendTelegramCsvData(const String& caption);

// Geradores de Relatórios e Mensagens Formatadas
String buildReportMessage();
String buildNetworkMessage(bool showFullSensitiveData = false);
String buildLogsMessage();
String buildHealthMessage();
String buildTrendMessage();
String buildStatsMessage();

// Ciclo Operacional de Polling e Inicialização
void pollTelegramCommands();
bool initTelegramBot();

// Interface com Display OLED Bicolor
void notifyOledTelegramTask(const String& cmdName, const String& taskDescription);
void setDisplayPinned(bool pinned);
bool isDisplayPinned();

#endif // TELEGRAM_BOT_H