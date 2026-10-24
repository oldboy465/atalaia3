#ifndef STORAGE_H
#define STORAGE_H

#include "Config.h"
#include <LittleFS.h>

// ============================================================================
// CONFIGURAÇÕES DE ENERGIA, ÁUDIO E DISPOSITIVOS VIRTUAIS
// ============================================================================
void loadAudioAndPowerConfig();
void saveAudioAndPowerConfig();

// ============================================================================
// GESTÃO DE CREDENCIAIS DE REDE NA FLASH
// ============================================================================
void loadWifiCredentials();
void saveWifiCredentials(const String& ssid, const String& pass);
void clearWifiCredentials();

// ============================================================================
// PERSISTÊNCIA RESILIENTE EM FLASH, METADADOS E BATCHING EM RAM
// ============================================================================
void initStorageSystem();
void loadMetadata();
void saveMetadataNow();
void saveSample(const SensorData &data);
void flushRamBatch();
int  getPendingBatchCount();
void resetBufferStorage();

// ============================================================================
// ANÁLISE ESTATÍSTICA E EXPORTAÇÃO DE TELEMETRIA COM LATITUDE E LONGITUDE
// ============================================================================
AverageStats calculateAverages(int maxSamples);
size_t exportStorageAsCsv(Print &output);

// ============================================================================
// LOGS DE EVENTOS, SINCRONISMO HORÁRIO E GEOLOCALIZAÇÃO
// ============================================================================
void addEventLog(const String& category, const String& message, uint8_t severity = 0);
String getFormattedDateTime();
void syncNtpTime();
void fetchGeoLocation();

// ============================================================================
// GERENCIAMENTO DE ENERGIA E SUSPENSÃO SEGURA
// ============================================================================
void setSystemStandby(bool enableStandby, bool notifyTelegram = true);
void enterSafeDeepSleep();

#endif // STORAGE_H