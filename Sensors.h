#ifndef SENSORS_H
#define SENSORS_H

#include "Config.h"
#include "AtalaiaModel.h"
#include <DHT.h>

extern DHT dht;

// Inicialização e Leitura de Hardware com Proteção
void initSensors();
int  readAdcOversampled(uint8_t pin, int samples = 16);
bool updateFlameStatusNonBlocking();
bool readDHT(float &temp, float &hum, DataQuality &quality);

// Modelagem Bioclimática, Higrotérmica e AQI com Compensação Dinâmica
float getCorrectionFactorMQ135(float tempC, float humPct);
float calculateCompensatedPPM(int rawAdc, float tempC, float humPct, float &outRsRoRatio);
float calculateRawPPM(int rawAdc);
int   calculateAQI(float ppm);
float calculateHeatIndex(float tempC, float humPct);
float calculateDewPoint(float tempC, float humPct);
String getAirQualityLabel(float ppm);
String getAQICategory(int aqi);

// Análise Estatística Preditiva e Histerese Multietapa
float computeThermalDerivative(float currentTemp);
RiskLevel evaluateSystemRisk(float ppm, float tempRate, bool flame, bool rain);

// Rotina Executiva de Amostragem Periódica
void executePeriodicSampling();

#endif // SENSORS_H