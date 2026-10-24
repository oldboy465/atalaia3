#ifndef UNICODE_GRAPHICS_H
#define UNICODE_GRAPHICS_H

#include <Arduino.h>

/**
 * @brief Motor de Renderização de Telemetria Tática com Caracteres Gráficos Unicode.
 * Gera diagramas sparklines, barras de nível proporcionais e caixas estruturadas.
 */
class UnicodeGraphics {
public:
  // Gera uma barra de progresso horizontal: [████████░░░░] 65%
  static String progressBar(float percentage, int width = 12) {
    float p = constrain(percentage, 0.0f, 100.0f);
    int filled = (int)round((p / 100.0f) * width);
    if (filled > width) filled = width;

    String bar = "[";
    for (int i = 0; i < filled; i++) {
      bar += "\u2588"; // Bloco sólido: █
    }
    for (int i = filled; i < width; i++) {
      bar += "\u2591"; // Bloco tracejado leve: ░
    }
    bar += "] " + String((int)p) + "%";
    return bar;
  }

  // Gera um mini-gráfico temporal sparkline a partir de um array numérico
  // Mapeia nas 8 alturas Unicode:   ▂ ▃ ▄ ▅ ▆ ▇ █
  static String sparkline(const float* values, int count, float forcedMin = -999.0f, float forcedMax = -999.0f) {
    if (count <= 0 || values == nullptr) return "--";

    static const char* const blocks[] = {
      " ", "\u2581", "\u2582", "\u2583", "\u2584", "\u2585", "\u2586", "\u2587", "\u2588"
    };

    float minVal = values[0];
    float maxVal = values[0];

    for (int i = 1; i < count; i++) {
      if (values[i] < minVal) minVal = values[i];
      if (values[i] > maxVal) maxVal = values[i];
    }

    if (forcedMin != -999.0f) minVal = forcedMin;
    if (forcedMax != -999.0f) maxVal = forcedMax;

    float range = maxVal - minVal;
    if (range <= 0.0001f) range = 1.0f;

    String out = "";
    for (int i = 0; i < count; i++) {
      float norm = (values[i] - minVal) / range;
      norm = constrain(norm, 0.0f, 1.0f);
      int index = (int)round(norm * 8.0f);
      if (index < 0) index = 0;
      if (index > 8) index = 8;
      out += blocks[index];
    }
    return out;
  }

  // Cria um cabeçalho emoldurado tático Unicode
  static String frameTitle(const String& title, const String& subtitle = "") {
    String out = "╔════════════════════════════════════╗\n";
    out += "║ " + title;
    int pad1 = 34 - title.length();
    for (int i = 0; i < pad1; i++) out += " ";
    out += " ║\n";

    if (subtitle.length() > 0) {
      out += "║ " + subtitle;
      int pad2 = 34 - subtitle.length();
      for (int i = 0; i < pad2; i++) out += " ";
      out += " ║\n";
    }
    out += "╚════════════════════════════════════╝\n";
    return out;
  }
};

#endif // UNICODE_GRAPHICS_H