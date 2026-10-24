#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "Config.h"
#include "images.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================================
// ATALAIA 3 — GESTOR DO ECRÃ OLED BICOLOR (SSD1306 128x64 I2C)
// Topo Físico Amarelo (0 a 15) | Base Física Azul (16 a 63)
// ============================================================================

void initDisplay();
void updateDisplayNonBlocking();
void showSplashImage(unsigned long durationMs = 5000);

// ============================================================================
// NAVEGAÇÃO, ÍNDICE DE TELAS E FIXAÇÃO
// ============================================================================
void nextDisplayScreen();
void previousDisplayScreen();
void selectDisplayScreen(uint8_t screenIndex);
uint8_t getCurrentDisplayScreen();
uint8_t getTotalDisplayScreens();
const char* getDisplayScreenName(uint8_t screenIndex);

void setDisplayPinned(bool pinned);
bool isDisplayPinned();
void toggleDisplayPin();

// ============================================================================
// GESTÃO DINÂMICA DE IMAGENS E ANIMAÇÃO NATALÍCIA SEM BLOQUEIO
// ============================================================================
void triggerDisplayCustomImage(uint8_t imageIndex, unsigned long durationMs);
void stopDisplayCustomImage();
void triggerDisplayChristmasAnimation(unsigned long durationMs = 8000);

// ============================================================================
// TERMINAL DE COMANDOS E MENSAGENS TELEGRAM
// ============================================================================
void appendTerminalCmdLine(const String& line);
void pushTelegramMessageToDisplay(const String& sender, const String& message);

// Acionadores de animações e popups
void triggerDisplayMatrixAnimation(unsigned long durationMs = OLED_MATRIX_DURATION_MS);
void triggerDisplayTaskPopup(const String& taskName, const String& details = "", unsigned long holdMs = OLED_POPUP_HOLD_MS);
void triggerDisplayHackerTerminal(const String& message);

// Renderizadores de páginas e elementos gráficos
void renderScrollerBanner();
void renderTelemetryDashboard();
void renderGraphScreen();
void renderNetworkScreen();
void renderPsychrometricScreen();
void renderTinyMLHealthScreen();
void renderMatrixRain();
void renderTaskPopup();
void renderHackerTerminal();
void renderPhoneTerminalScreen();
void renderCriticalFlameAlert();
void renderChristmasAnimation();
void renderCustomImageScreen();

void drawPhoneStatusBarYellow();
void drawCmdTerminalBlue();
void drawWifiSignalIcon(int16_t x, int16_t y, int8_t rssi);

void pushTelemetryToOledHistory(float temp, float hum, float gas);
void drawMiniSparkline(int16_t x, int16_t y, int16_t w, int16_t h, const float* data, int count, float minVal, float maxVal);

#endif // DISPLAY_MANAGER_H