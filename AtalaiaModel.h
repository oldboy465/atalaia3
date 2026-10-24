#ifndef ATALAIA_MODEL_H
#define ATALAIA_MODEL_H

#include "Config.h"

/**
 * @brief Classificador Nativo de Borda TinyML para Avaliação de Risco Multidimensional.
 * 
 * Implementação em árvore de decisão compilada baremetal (Zero-Allocation, O(depth)).
 * Entradas Normalizadas:
 *   - temp: Temperatura higrotérmica (°C)
 *   - hum: Umidade Relativa do Ar (%)
 *   - mq_ratio: Razão higrotérmica corrigida Rs/R0 do MQ-135
 *   - rain_adc: Leitura direta do canal ADC do sensor condutivo de chuva (0 a 4095)
 *   - dt_rate: Derivada temporal de temperatura dT/dt (°C/min)
 */
class AtalaiaModel {
public:
  static inline MLState predict(float temp, float hum, float mq_ratio, int rain_adc, float dt_rate) {
    // 1. Ramo Convectivo / Precipitação Iminente
    if (rain_adc < 1900) {
      return ML_CHUVA_CONVECTIVA;
    }

    if (rain_adc < 2400 && hum > 82.0f) {
      return ML_CHUVA_CONVECTIVA;
    }

    // 2. Ramo Preditivo de Anomalia Térmica / Pré-Combustão
    if (dt_rate >= THERMAL_RATE_TRIGGER_C_MIN && temp >= 34.0f) {
      return ML_ANOMALIA_PRE_COMBUSTAO;
    }

    if (mq_ratio < 0.38f && temp >= 38.0f) {
      return ML_ANOMALIA_PRE_COMBUSTAO;
    }

    if (temp >= 52.0f) {
      return ML_ANOMALIA_PRE_COMBUSTAO;
    }

    // 3. Ramo Psicrométrico Crítico / Saturação de Ponto de Orvalho
    if (hum >= 93.0f && temp <= 27.0f) {
      return ML_CONDENSACAO_ORVALHO_CRIT;
    }

    float dewDelta = temp - ((temp - (100.0f - hum) / 5.0f));
    if (dewDelta < 1.2f && hum > 88.0f) {
      return ML_CONDENSACAO_ORVALHO_CRIT;
    }

    // 4. Estado Nominal Estável
    return ML_AMBIENTE_ESTAVEL;
  }

  static inline const char* getStateName(MLState state) {
    switch (state) {
      case ML_AMBIENTE_ESTAVEL:
        return "AMBIENTE_ESTAVEL";
      case ML_CONDENSACAO_ORVALHO_CRIT:
        return "CONDENSACAO_ORVALHO_CRIT";
      case ML_ANOMALIA_PRE_COMBUSTAO:
        return "ANOMALIA_PRE_COMBUSTAO";
      case ML_CHUVA_CONVECTIVA:
        return "CHUVA_CONVECTIVA";
      default:
        return "DESCONHECIDO";
    }
  }

  static inline const char* getStateDescription(MLState state) {
    switch (state) {
      case ML_AMBIENTE_ESTAVEL:
        return "Parametros estaveis dentro do envelope operacional.";
      case ML_CONDENSACAO_ORVALHO_CRIT:
        return "Ponto de orvalho critico com iminencia de condensacao liquida.";
      case ML_ANOMALIA_PRE_COMBUSTAO:
        return "Gradiente termico anormal ou gaseificacao compativel com pre-fogo.";
      case ML_CHUVA_CONVECTIVA:
        return "Laminacao de agua ativa ou saturacao condutiva por precipitacao.";
      default:
        return "Indeterminado.";
    }
  }

  static inline const char* getStateBadgeColor(MLState state) {
    switch (state) {
      case ML_AMBIENTE_ESTAVEL:
        return "#10b981"; // Verde Esmeralda
      case ML_CONDENSACAO_ORVALHO_CRIT:
        return "#0284c7"; // Azul Celeste
      case ML_ANOMALIA_PRE_COMBUSTAO:
        return "#ef4444"; // Vermelho Alerta
      case ML_CHUVA_CONVECTIVA:
        return "#8b5cf6"; // Violeta Convectivo
      default:
        return "#64748b"; // Cinza
    }
  }
};

#endif // ATALAIA_MODEL_H