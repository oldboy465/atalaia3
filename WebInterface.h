#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include "Config.h"

extern const char INDEX_HTML[] PROGMEM;

// ============================================================================
// GESTÃO DO SERVIDOR WEB ASSÍNCRONO E PORTAL CATIVO
// ============================================================================
bool handleCaptivePortal();
void setupWebServer();

#endif // WEB_INTERFACE_H