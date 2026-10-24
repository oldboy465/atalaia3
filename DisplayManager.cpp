#include "DisplayManager.h"
#include "Sensors.h"
#include "Diagnostics.h"
#include "Storage.h"
#include "AtalaiaModel.h"
#include "images.h"
#include <Wire.h>

static Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
static bool displayReady = false;

// Estado da FSM de Visualização
static DisplayMode currentDisplayMode = DISP_MODE_CAROUSEL;
bool isDisplayPinnedState = false;
bool showIpOnDisplay = true;

uint8_t carouselPageIndex = 0;
const uint8_t TOTAL_CAROUSEL_PAGES = 6; // 0: Dash, 1: CMD, 2: Graficos, 3: Rede, 4: Orvalho/Sensacao, 5: TinyML/CPU

static const char* SCREEN_NAMES[TOTAL_CAROUSEL_PAGES] = {
  "Dashboard Geral",
  "Terminal CMD",
  "Graficos Historicos",
  "Diagnostico de Rede",
  "Clima e Orvalho",
  "TinyML e Hardware"
};

static unsigned long lastPageSwitchTime = 0;
static unsigned long popupEndTime = 0;
static unsigned long matrixEndTime = 0;

static String activeTaskTitle = "";
static String activeTaskDetails = "";

static String hackerFullText = "";
static size_t hackerCharIndex = 0;
static unsigned long lastTypewriterTime = 0;
static unsigned long hackerCursorBlinkTimer = 0;
static bool hackerCursorState = true;
static unsigned long hackerHoldEndTime = 0;

// Buffer de linhas do terminal azul (linhas 16 a 63)
static String terminalLines[OLED_TERMINAL_LINE_MAX];
static uint8_t terminalLineCount = 0;
static unsigned long terminalCursorBlink = 0;
static bool terminalCursorState = true;

static const char MARQUEE_TEXT[] = "  * ATALAIA 3 - O VIGILANTE * by Philipe *  ";
static int16_t scrollerX = SCREEN_WIDTH;
static unsigned long lastScrollerTime = 0;

#define GRAPH_POINTS 32
static float histTemp[GRAPH_POINTS];
static float histHum[GRAPH_POINTS];
static float histGasRaw[GRAPH_POINTS];
static int histCount = 0;

#define MATRIX_COLS 16
struct MatrixDrop {
  int8_t y;
  int8_t speed;
  char   glyph;
};
static MatrixDrop matrixDrops[MATRIX_COLS];
static unsigned long lastMatrixStep = 0;

// Variáveis de controle de imagens modulares e animação natalícia
static uint8_t activeImageIndex = 0;
static unsigned long customImageEndTime = 0;
static unsigned long christmasAnimEndTime = 0;
static uint8_t christmasFrame = 0;
static unsigned long lastChristmasFrameTime = 0;

void nextDisplayScreen() {
  carouselPageIndex = (carouselPageIndex + 1) % TOTAL_CAROUSEL_PAGES;
  lastPageSwitchTime = millis();
}

void previousDisplayScreen() {
  carouselPageIndex = (carouselPageIndex == 0) ? (TOTAL_CAROUSEL_PAGES - 1) : (carouselPageIndex - 1);
  lastPageSwitchTime = millis();
}

void selectDisplayScreen(uint8_t screenIndex) {
  if (screenIndex < TOTAL_CAROUSEL_PAGES) {
    carouselPageIndex = screenIndex;
    lastPageSwitchTime = millis();
  }
}

uint8_t getCurrentDisplayScreen() {
  return carouselPageIndex;
}

uint8_t getTotalDisplayScreens() {
  return TOTAL_CAROUSEL_PAGES;
}

const char* getDisplayScreenName(uint8_t screenIndex) {
  if (screenIndex < TOTAL_CAROUSEL_PAGES) {
    return SCREEN_NAMES[screenIndex];
  }
  return "Desconhecida";
}

void setDisplayPinned(bool pinned) {
  isDisplayPinnedState = pinned;
  if (isDisplayPinnedState) {
    currentDisplayMode = DISP_MODE_PHONE_TERMINAL;
  } else {
    currentDisplayMode = DISP_MODE_CAROUSEL;
    lastPageSwitchTime = millis();
  }
}

bool isDisplayPinned() {
  return isDisplayPinnedState;
}

void toggleDisplayPin() {
  setDisplayPinned(!isDisplayPinnedState);
}

void triggerDisplayCustomImage(uint8_t imageIndex, unsigned long durationMs) {
  if (SYSTEM_IMAGE_COUNT == 0) return;
  activeImageIndex = (imageIndex < SYSTEM_IMAGE_COUNT) ? imageIndex : 0;
  customImageEndTime = millis() + durationMs;
  currentDisplayMode = DISP_MODE_CUSTOM_IMAGE;
}

void stopDisplayCustomImage() {
  if (currentDisplayMode == DISP_MODE_CUSTOM_IMAGE) {
    currentDisplayMode = isDisplayPinnedState ? DISP_MODE_PHONE_TERMINAL : DISP_MODE_CAROUSEL;
    lastPageSwitchTime = millis();
  }
}

void triggerDisplayChristmasAnimation(unsigned long durationMs) {
  christmasAnimEndTime = millis() + durationMs;
  christmasFrame = 0;
  lastChristmasFrameTime = millis();
  currentDisplayMode = DISP_MODE_CHRISTMAS_ANIM;
}

void appendTerminalCmdLine(const String& line) {
  if (terminalLineCount < OLED_TERMINAL_LINE_MAX) {
    terminalLines[terminalLineCount++] = line;
  } else {
    for (uint8_t i = 0; i < OLED_TERMINAL_LINE_MAX - 1; i++) {
      terminalLines[i] = terminalLines[i + 1];
    }
    terminalLines[OLED_TERMINAL_LINE_MAX - 1] = line;
  }
}

void pushTelegramMessageToDisplay(const String& sender, const String& message) {
  String formatted = "[" + sender + "] " + message;
  int startIdx = 0;
  while (startIdx < (int)formatted.length()) {
    int len = min((int)formatted.length() - startIdx, OLED_TERMINAL_CHAR_LIMIT);
    appendTerminalCmdLine(formatted.substring(startIdx, startIdx + len));
    startIdx += len;
  }

  if (!isDisplayPinnedState) {
    currentDisplayMode = DISP_MODE_PHONE_TERMINAL;
    lastPageSwitchTime = millis();
  }
}

void drawWifiSignalIcon(int16_t x, int16_t y, int8_t rssi) {
  if (isApMode) {
    oled.setTextSize(1);
    oled.setCursor(x, y);
    oled.print("AP");
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    oled.drawLine(x, y, x + 8, y + 8, SSD1306_WHITE);
    oled.drawLine(x + 8, y, x, y + 8, SSD1306_WHITE);
    return;
  }

  int bars = 1;
  if (rssi >= -60) bars = 4;
  else if (rssi >= -72) bars = 3;
  else if (rssi >= -84) bars = 2;

  for (int i = 0; i < 4; i++) {
    int16_t h = (i + 1) * 2;
    int16_t bx = x + (i * 3);
    int16_t by = y + (8 - h);
    if (i < bars) {
      oled.fillRect(bx, by, 2, h, SSD1306_WHITE);
    } else {
      oled.drawFastHLine(bx, y + 7, 2, SSD1306_WHITE);
    }
  }
}

void drawPhoneStatusBarYellow() {
  drawWifiSignalIcon(2, 3, WiFi.RSSI());

  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  oled.setCursor(18, 3);
  oled.printf("%.1fC", currentData.temperature);

  oled.setCursor(56, 3);
  oled.printf("%.0f%%", currentData.humidity);

  oled.setCursor(92, 3);
  if (isDisplayPinnedState) {
    oled.print("[FIXO]");
  } else {
    struct tm timeinfo;
    if (ntpSynced && getLocalTime(&timeinfo, 5)) {
      oled.printf("%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    } else {
      oled.printf("%lums", (millis() / 1000UL) % 100);
    }
  }

  oled.drawFastHLine(0, 15, 128, SSD1306_WHITE);
}

void drawCmdTerminalBlue() {
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  int startY = 17;
  for (uint8_t i = 0; i < terminalLineCount; i++) {
    oled.setCursor(1, startY + (i * 9));
    oled.print(terminalLines[i]);
  }

  if (millis() - terminalCursorBlink >= 350) {
    terminalCursorBlink = millis();
    terminalCursorState = !terminalCursorState;
  }

  if (terminalCursorState && terminalLineCount < OLED_TERMINAL_LINE_MAX) {
    int cursorY = startY + (terminalLineCount * 9);
    if (cursorY <= 55) {
      oled.setCursor(1, cursorY);
      oled.print("C:\\> _");
    }
  }
}

void renderPhoneTerminalScreen() {
  drawPhoneStatusBarYellow();
  drawCmdTerminalBlue();
}

void showSplashImage(unsigned long durationMs) {
  if (!displayReady || SYSTEM_IMAGE_COUNT == 0) return;

  oled.clearDisplay();

  const int targetW = 81;
  const int targetH = 64;
  const int offsetX = (SCREEN_WIDTH - targetW) / 2;
  const int offsetY = 0;

  int srcW = SYSTEM_IMAGES[0].width;
  int srcH = SYSTEM_IMAGES[0].height;
  const uint8_t* imgPtr = SYSTEM_IMAGES[0].data;

  for (int y = 0; y < targetH; y++) {
    int srcY = (y * srcH) / targetH;
    int page = srcY / 8;
    int bitMask = 1 << (srcY % 8);

    for (int x = 0; x < targetW; x++) {
      int srcX = (x * srcW) / targetW;
      int byteIdx = page * srcW + srcX;

      uint8_t b = pgm_read_byte(&imgPtr[byteIdx]);
      if (b & bitMask) {
        oled.drawPixel(offsetX + x, offsetY + y, SSD1306_WHITE);
      } else {
        oled.drawPixel(offsetX + x, offsetY + y, SSD1306_BLACK);
      }
    }
  }

  oled.display();

  if (durationMs > 0) {
    delay(durationMs);
  }
}

void renderCustomImageScreen() {
  if (SYSTEM_IMAGE_COUNT == 0) return;
  if (activeImageIndex >= SYSTEM_IMAGE_COUNT) activeImageIndex = 0;

  const int targetW = 81;
  const int targetH = 64;
  const int offsetX = (SCREEN_WIDTH - targetW) / 2;
  const int offsetY = 0;

  int srcW = SYSTEM_IMAGES[activeImageIndex].width;
  int srcH = SYSTEM_IMAGES[activeImageIndex].height;
  const uint8_t* imgPtr = SYSTEM_IMAGES[activeImageIndex].data;

  for (int y = 0; y < targetH; y++) {
    int srcY = (y * srcH) / targetH;
    int page = srcY / 8;
    int bitMask = 1 << (srcY % 8);

    for (int x = 0; x < targetW; x++) {
      int srcX = (x * srcW) / targetW;
      int byteIdx = page * srcW + srcX;

      uint8_t b = pgm_read_byte(&imgPtr[byteIdx]);
      if (b & bitMask) {
        oled.drawPixel(offsetX + x, offsetY + y, SSD1306_WHITE);
      } else {
        oled.drawPixel(offsetX + x, offsetY + y, SSD1306_BLACK);
      }
    }
  }

  oled.fillRect(0, 55, 128, 9, SSD1306_BLACK);
  oled.drawFastHLine(0, 54, 128, SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(2, 56);
  oled.printf("[%u/%u] %s", activeImageIndex + 1, SYSTEM_IMAGE_COUNT, SYSTEM_IMAGES[activeImageIndex].name);
}

void renderChristmasAnimation() {
  unsigned long now = millis();
  if (now - lastChristmasFrameTime >= 200) {
    lastChristmasFrameTime = now;
    christmasFrame = (christmasFrame + 1) % 4;
  }

  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(16, 2);
  oled.print("* FELIZ NATAL *");
  oled.drawFastHLine(0, 11, 128, SSD1306_WHITE);

  // Árvore de Natal Vetorial
  oled.drawTriangle(26, 14, 14, 28, 38, 28, SSD1306_WHITE);
  oled.drawTriangle(26, 24, 10, 42, 42, 42, SSD1306_WHITE);
  oled.drawTriangle(26, 36, 6,  54, 46, 54, SSD1306_WHITE);
  oled.fillRect(23, 54, 6, 8, SSD1306_WHITE);

  if (christmasFrame % 2 == 0) {
    oled.drawPixel(26, 12, SSD1306_WHITE);
    oled.drawPixel(25, 13, SSD1306_WHITE);
    oled.drawPixel(27, 13, SSD1306_WHITE);
  }

  oled.setCursor(54, 18);
  oled.print("JINGLE");
  oled.setCursor(54, 28);
  oled.print("BELLS!");

  oled.drawRect(52, 40, 72, 20, SSD1306_WHITE);
  oled.setCursor(56, 46);
  if (christmasFrame == 0) oled.print("SIRENE: 1");
  else if (christmasFrame == 1) oled.print("MELODIA: 2");
  else if (christmasFrame == 2) oled.print("AUDIO: OK");
  else oled.print("ATALAIA 3");

  for (int i = 0; i < 8; i++) {
    int snowX = (i * 16 + (christmasFrame * 5)) % 128;
    int snowY = (16 + (i * 7) + (christmasFrame * 8)) % 60;
    oled.drawPixel(snowX, snowY, SSD1306_WHITE);
  }
}

void initDisplay() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(400000);

  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
    Serial.println("[OLED] Falha ao inicializar SSD1306!");
    displayReady = false;
    return;
  }

  displayReady = true;
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextWrap(false);

  showSplashImage(5000);

  for (int i = 0; i < MATRIX_COLS; i++) {
    matrixDrops[i].y = random(-16, 0);
    matrixDrops[i].speed = random(1, 4);
    matrixDrops[i].glyph = (char)random(33, 126);
  }

  for (int i = 0; i < GRAPH_POINTS; i++) {
    histTemp[i]   = 25.0f;
    histHum[i]    = 50.0f;
    histGasRaw[i] = 1000.0f;
  }
  histCount = 1;

  appendTerminalCmdLine("ATALAIA 3 OS v3.3");
  appendTerminalCmdLine("Kernel Dual-Core OK");
  appendTerminalCmdLine("OLED SSD1306 Ativo");
  appendTerminalCmdLine("Aguardando Telegram");

  oled.clearDisplay();
  oled.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  oled.drawRect(2, 2, 124, 60, SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(18, 14);
  oled.print(">> ATALAIA 3 <<");
  oled.setCursor(14, 28);
  oled.print("O VIGILANTE EDGE");
  oled.setCursor(12, 44);
  oled.print("SISTEMA EMBARCADO");
  oled.display();
  delay(1200);

  triggerDisplayMatrixAnimation(3000);
}

void pushTelemetryToOledHistory(float temp, float hum, float gas) {
  if (histCount < GRAPH_POINTS) {
    histTemp[histCount]   = temp;
    histHum[histCount]    = hum;
    histGasRaw[histCount] = gas;
    histCount++;
  } else {
    for (int i = 0; i < GRAPH_POINTS - 1; i++) {
      histTemp[i]   = histTemp[i + 1];
      histHum[i]    = histHum[i + 1];
      histGasRaw[i] = histGasRaw[i + 1];
    }
    histTemp[GRAPH_POINTS - 1]   = temp;
    histHum[GRAPH_POINTS - 1]    = hum;
    histGasRaw[GRAPH_POINTS - 1] = gas;
  }
}

void triggerDisplayMatrixAnimation(unsigned long durationMs) {
  currentDisplayMode = DISP_MODE_MATRIX_RAIN;
  matrixEndTime = millis() + durationMs;
  for (int i = 0; i < MATRIX_COLS; i++) {
    matrixDrops[i].y = random(-20, 0);
    matrixDrops[i].speed = random(2, 5);
  }
}

void triggerDisplayTaskPopup(const String& taskName, const String& details, unsigned long holdMs) {
  activeTaskTitle = taskName;
  activeTaskDetails = details;
  popupEndTime = millis() + holdMs;
  currentDisplayMode = DISP_MODE_TASK_POPUP;
}

void triggerDisplayHackerTerminal(const String& message) {
  hackerFullText = message;
  hackerCharIndex = 0;
  lastTypewriterTime = millis();
  hackerHoldEndTime = millis() + 14000UL;
  currentDisplayMode = DISP_MODE_HACKER_TERMINAL;
}

void renderScrollerBanner() {
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(scrollerX, 0);
  oled.print(MARQUEE_TEXT);

  oled.drawFastHLine(0, 9, 128, SSD1306_WHITE);

  if (millis() - lastScrollerTime >= 35) {
    lastScrollerTime = millis();
    scrollerX -= 2;
    int textWidth = strlen(MARQUEE_TEXT) * 6;
    if (scrollerX < -textWidth) {
      scrollerX = SCREEN_WIDTH;
    }
  }
}

// Tela 1: Dashboard Geral
void renderTelemetryDashboard() {
  renderScrollerBanner();

  oled.setCursor(0, 12);
  oled.printf("T:%.1fC", currentData.temperature);
  oled.setCursor(68, 12);
  oled.printf("U:%.0f%%", currentData.humidity);

  int aqi = calculateAQI(currentData.mq135_ppm_comp);
  oled.setCursor(0, 24);
  oled.printf("MQ:%d", currentData.mq135_raw);
  oled.setCursor(76, 24);
  oled.printf("AQI:%d", aqi);

  oled.setCursor(0, 36);
  oled.printf("CHUVA:%s", currentData.rain_detected ? "SIM" : "NAO");
  oled.setCursor(70, 36);
  oled.printf("FOGO:%s", currentData.flame_detected ? "!SIM!" : "OFF");

  oled.drawFastHLine(0, 48, 128, SSD1306_WHITE);
  oled.setCursor(0, 52);
  oled.printf("[%u/%u] %s", carouselPageIndex + 1, TOTAL_CAROUSEL_PAGES, getRiskLevelName((RiskLevel)currentData.risk_level));
  oled.setCursor(78, 52);
  oled.printf("UP:%lus", currentData.uptime_sec);
}

void drawMiniSparkline(int16_t x, int16_t y, int16_t w, int16_t h, const float* data, int count, float minVal, float maxVal) {
  oled.drawRect(x, y, w, h, SSD1306_WHITE);
  if (count < 2) return;

  float range = maxVal - minVal;
  if (range <= 0.001f) range = 1.0f;

  float stepX = (float)(w - 4) / (float)(count - 1);

  for (int i = 0; i < count - 1; i++) {
    float n1 = constrain((data[i] - minVal) / range, 0.0f, 1.0f);
    float n2 = constrain((data[i + 1] - minVal) / range, 0.0f, 1.0f);

    int16_t py1 = (y + h - 2) - (int16_t)(n1 * (h - 4));
    int16_t py2 = (y + h - 2) - (int16_t)(n2 * (h - 4));
    int16_t px1 = x + 2 + (int16_t)(i * stepX);
    int16_t px2 = x + 2 + (int16_t)((i + 1) * stepX);

    oled.drawLine(px1, py1, px2, py2, SSD1306_WHITE);
  }
}

// Tela 3: Gráficos
void renderGraphScreen() {
  renderScrollerBanner();

  oled.setTextSize(1);
  oled.setCursor(0, 11);
  oled.printf("[%u/%u] GRAFICOS SERIE TEMPO", carouselPageIndex + 1, TOTAL_CAROUSEL_PAGES);

  drawMiniSparkline(0, 22, 62, 30, histTemp, histCount, 15.0f, 45.0f);
  oled.setCursor(2, 54);
  oled.printf("T:%.1fC", currentData.temperature);

  drawMiniSparkline(66, 22, 62, 30, histGasRaw, histCount, 200.0f, 3800.0f);
  oled.setCursor(68, 54);
  oled.printf("MQ:%d", currentData.mq135_raw);
}

// Tela 4: Diagnóstico de Rede
void renderNetworkScreen() {
  renderScrollerBanner();

  oled.setTextSize(1);
  oled.setCursor(0, 12);

  if (isApMode) {
    oled.print("MODO: SOFT-AP PROPRIO");
    oled.setCursor(0, 24);
    oled.printf("SSID: %s", AP_SSID);

    uint8_t connectedStations = WiFi.softAPgetStationNum();
    oled.setCursor(0, 36);
    oled.printf("CLIENTES: %u", connectedStations);

    oled.setCursor(0, 48);
    if (showIpOnDisplay) {
      oled.printf("IP: %s", WiFi.softAPIP().toString().c_str());
    } else {
      oled.print("IP: [PROTEGIDO]");
    }

    for (int i = 0; i < 4; i++) {
      if (i < connectedStations) {
        oled.fillRect(108 + (i * 5), 36, 4, 8, SSD1306_WHITE);
      } else {
        oled.drawRect(108 + (i * 5), 36, 4, 8, SSD1306_WHITE);
      }
    }
  } else {
    oled.print("MODO: STATION (ONLINE)");
    oled.setCursor(0, 24);
    oled.printf("SSID: %s", WiFi.SSID().substring(0, 14).c_str());
    oled.setCursor(0, 36);
    if (showIpOnDisplay) {
      oled.printf("IP: %s", WiFi.localIP().toString().c_str());
    } else {
      oled.print("IP: [PROTEGIDO]");
    }
    oled.setCursor(0, 48);
    oled.printf("SINAL: %d dBm (%u%%)", WiFi.RSSI(), calculateSignalQuality(WiFi.RSSI()));
  }
}

// Tela 5 (NOVA): Psicrometria, Ponto de Orvalho e Derivada Térmica
void renderPsychrometricScreen() {
  renderScrollerBanner();

  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  oled.setCursor(0, 12);
  oled.printf("SENS.TERMICA : %.1f C", currentData.heat_index);

  oled.setCursor(0, 24);
  oled.printf("PTO ORVALHO  : %.1f C", currentData.dew_point);

  oled.setCursor(0, 36);
  oled.printf("TAXA dT/dt   : %+.2f C/m", currentData.temp_rate_minute);

  oled.drawFastHLine(0, 48, 128, SSD1306_WHITE);
  oled.setCursor(0, 52);
  oled.printf("[%u/%u] PSICROMETRIA", carouselPageIndex + 1, TOTAL_CAROUSEL_PAGES);
  oled.setCursor(96, 52);
  oled.print(currentData.data_quality == DATA_VALID ? "OK" : "SUSP");
}

// Tela 6 (NOVA): Motor TinyML, Nível de Risco e Saúde do Silício
void renderTinyMLHealthScreen() {
  renderScrollerBanner();

  SystemHealth health = getSystemHealth();

  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  oled.setCursor(0, 12);
  oled.printf("ML: %s", AtalaiaModel::getStateName((MLState)currentData.ml_state));

  oled.setCursor(0, 24);
  oled.printf("RISCO: %s", getRiskLevelName((RiskLevel)currentData.risk_level));

  oled.setCursor(0, 36);
  oled.printf("CPU:%uMHz  T:%.1fC", health.cpuFreqMHz, health.cpuTemperature);

  oled.drawFastHLine(0, 48, 128, SSD1306_WHITE);
  oled.setCursor(0, 52);
  oled.printf("[%u/%u] MEM:%uK", carouselPageIndex + 1, TOTAL_CAROUSEL_PAGES, (unsigned int)(health.freeHeap / 1024));
  oled.setCursor(96, 52);
  oled.printf("FOG:%s", flameActiveLow ? "LOW" : "HIGH");
}

void renderMatrixRain() {
  oled.setTextSize(1);
  for (int i = 0; i < MATRIX_COLS; i++) {
    int16_t x = i * 8;
    int16_t y = matrixDrops[i].y;

    if (y >= 0 && y < SCREEN_HEIGHT) {
      oled.drawChar(x, y, matrixDrops[i].glyph, SSD1306_WHITE, SSD1306_BLACK, 1);
    }
    if (y - 8 >= 0 && y - 8 < SCREEN_HEIGHT) {
      oled.drawChar(x, y - 8, (char)random(33, 126), SSD1306_WHITE, SSD1306_BLACK, 1);
    }

    if (millis() - lastMatrixStep >= 40) {
      matrixDrops[i].y += matrixDrops[i].speed;
      if (matrixDrops[i].y > SCREEN_HEIGHT + 10) {
        matrixDrops[i].y = random(-16, 0);
        matrixDrops[i].speed = random(2, 6);
        matrixDrops[i].glyph = (char)random(65, 122);
      }
    }
  }

  oled.fillRect(10, 22, 108, 20, SSD1306_BLACK);
  oled.drawRect(10, 22, 108, 20, SSD1306_WHITE);
  oled.setCursor(16, 28);
  oled.print("CONEXAO ONLINE!");

  if (millis() - lastMatrixStep >= 40) {
    lastMatrixStep = millis();
  }
}

void renderTaskPopup() {
  oled.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  oled.drawRect(2, 2, 124, 60, SSD1306_WHITE);

  oled.fillRect(2, 2, 124, 14, SSD1306_WHITE);
  oled.setTextColor(SSD1306_BLACK);
  oled.setTextSize(1);
  oled.setCursor(8, 5);
  oled.print("ATALAIA COMANDO");

  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(6, 22);
  oled.printf("Tarefa: %s", activeTaskTitle.substring(0, 14).c_str());

  oled.setCursor(6, 36);
  if (activeTaskDetails.length() > 0) {
    oled.printf("Status: %s", activeTaskDetails.substring(0, 14).c_str());
  } else {
    oled.print("Status: EXECUTANDO...");
  }

  static uint8_t prog = 0;
  prog = (prog + 8) % 110;
  oled.drawRect(6, 49, 114, 8, SSD1306_WHITE);
  oled.fillRect(8, 51, prog, 4, SSD1306_WHITE);
}

void renderHackerTerminal() {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  oled.setCursor(0, 0);
  oled.print("root@atalaia3:~# cmd");
  oled.drawFastHLine(0, 9, 128, SSD1306_WHITE);

  if (hackerCharIndex < hackerFullText.length()) {
    if (millis() - lastTypewriterTime >= 45) {
      lastTypewriterTime = millis();
      hackerCharIndex++;
    }
  }

  String currentSlice = hackerFullText.substring(0, hackerCharIndex);

  int cursorY = 12;
  int startIdx = 0;
  while (startIdx < (int)currentSlice.length() && cursorY < 56) {
    int endIdx = min((int)currentSlice.length(), startIdx + 21);
    oled.setCursor(0, cursorY);
    oled.print(currentSlice.substring(startIdx, endIdx));
    startIdx += 21;
    cursorY += 10;
  }

  if (millis() - hackerCursorBlinkTimer >= 300) {
    hackerCursorBlinkTimer = millis();
    hackerCursorState = !hackerCursorState;
  }

  if (hackerCursorState && cursorY <= 56) {
    oled.print("_");
  }

  if (millis() > hackerHoldEndTime && hackerCharIndex >= hackerFullText.length()) {
    currentDisplayMode = isDisplayPinnedState ? DISP_MODE_PHONE_TERMINAL : DISP_MODE_CAROUSEL;
  }
}

void renderCriticalFlameAlert() {
  static bool invertState = false;
  static unsigned long lastInvert = 0;

  if (millis() - lastInvert >= 120) {
    lastInvert = millis();
    invertState = !invertState;
  }

  if (invertState) {
    oled.fillRect(0, 0, 128, 64, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
  } else {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
  }

  oled.setTextSize(2);
  oled.setCursor(14, 8);
  oled.print("! FOGO !");
  oled.setTextSize(1);
  oled.setCursor(4, 30);
  oled.print("CHAMA DETECTADA D33");
  oled.setCursor(12, 44);
  oled.print("SIRENE EM CURSO");
}

void updateDisplayNonBlocking() {
  if (!displayReady) return;

  unsigned long now = millis();

  static unsigned long lastOledFrameTime = 0;
  if (now - lastOledFrameTime < 40UL) {
    return;
  }
  lastOledFrameTime = now;

  oled.clearDisplay();

  // 1. Prioridade Máxima: Alarme de Incêndio Ativo
  if (currentData.flame_detected == 1) {
    renderCriticalFlameAlert();
    oled.display();
    return;
  }

  // 2. Animação de Natal Não-Bloqueante
  if (currentDisplayMode == DISP_MODE_CHRISTMAS_ANIM) {
    if (now >= christmasAnimEndTime) {
      currentDisplayMode = isDisplayPinnedState ? DISP_MODE_PHONE_TERMINAL : DISP_MODE_CAROUSEL;
      lastPageSwitchTime = now;
    } else {
      renderChristmasAnimation();
      oled.display();
      return;
    }
  }

  // 3. Imagem Estática Personalizada Não-Bloqueante
  if (currentDisplayMode == DISP_MODE_CUSTOM_IMAGE) {
    if (now >= customImageEndTime) {
      currentDisplayMode = isDisplayPinnedState ? DISP_MODE_PHONE_TERMINAL : DISP_MODE_CAROUSEL;
      lastPageSwitchTime = now;
    } else {
      renderCustomImageScreen();
      oled.display();
      return;
    }
  }

  // 4. Modo Terminal Hacker
  if (currentDisplayMode == DISP_MODE_HACKER_TERMINAL) {
    renderHackerTerminal();
    oled.display();
    return;
  }

  // 5. Popups de Tarefas Curtas
  if (currentDisplayMode == DISP_MODE_TASK_POPUP) {
    if (now >= popupEndTime) {
      currentDisplayMode = isDisplayPinnedState ? DISP_MODE_PHONE_TERMINAL : DISP_MODE_CAROUSEL;
    } else {
      renderTaskPopup();
      oled.display();
      return;
    }
  }

  // 6. Efeito Matrix Rain
  if (currentDisplayMode == DISP_MODE_MATRIX_RAIN) {
    if (now >= matrixEndTime) {
      currentDisplayMode = isDisplayPinnedState ? DISP_MODE_PHONE_TERMINAL : DISP_MODE_CAROUSEL;
    } else {
      renderMatrixRain();
      oled.display();
      return;
    }
  }

  // 7. Tela Celular Fixada
  if (isDisplayPinnedState) {
    renderPhoneTerminalScreen();
    oled.display();
    return;
  }

  // 8. Carrossel Padrão com 6 Telas
  if (now - lastPageSwitchTime >= OLED_SCREEN_ROTATE_MS) {
    lastPageSwitchTime = now;
    carouselPageIndex = (carouselPageIndex + 1) % TOTAL_CAROUSEL_PAGES;
  }

  switch (carouselPageIndex) {
    case 0:
      renderTelemetryDashboard();
      break;
    case 1:
      renderPhoneTerminalScreen();
      break;
    case 2:
      renderGraphScreen();
      break;
    case 3:
      renderNetworkScreen();
      break;
    case 4:
      renderPsychrometricScreen();
      break;
    case 5:
      renderTinyMLHealthScreen();
      break;
    default:
      renderTelemetryDashboard();
      break;
  }

  oled.display();
}