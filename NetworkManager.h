#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "Config.h"

// ============================================================================
// PILHA DE GESTÃO DE REDE, PONTO DE ACESSO E CONECTIVIDADE RESILIENTE
// ============================================================================
void startAccessPoint();
bool connectStation(const String& ssid, const String& pass);
void setupNetworkStack();
void handleNetworkMaintenance();
bool isStationConnected();
int8_t getCurrentRssi();

#endif // NETWORK_MANAGER_H