# ATALAIA 3 - Estação Embarcada de Monitoramento e Riscos (Headless)

O **ATALAIA 3** é uma estação autônoma de sensoriamento ambiental e detecção de riscos críticos (chama, chuva e gases tóxicos/inflamáveis) baseada no microcontrolador ESP32. 

Nesta versão *headless* (sem display físico), o sistema elimina o timer de multiplexação e pinos dedicados a telas, centralizando toda a telemetria, diagnóstico manual e configuração de rede em um servidor web embarcado com suporte a **Captive Portal**, resolução mDNS local (`http://atalaia2.local`) e persistência em memória Flash via buffer circular.

---

## 🚀 Funcionalidades Principais

* **Sensoriamento Periódico (Ciclo de 5s):**
  * **Temperatura e Umidade:** Driver nativo *bit-banging* universal com decodificação automática (compatível com DHT11, DHT22 e AM2302).
  * **Gás Atmosférico:** Leitura analógica contínua da concentração de gases via módulo MQ-135.
  * **Detecção de Chuva:** Monitoramento analógico com threshold ajustado via software para identificar presença/ausência de água.
* **Segurança e Monitoramento Crítico de Incêndio:**
  * Sensor IR óptico com interrupção digital por hardware (`GPIO 33`).
  * Acionamento sonoro imediato de **Sirene Policial** modulada em frequência (900 Hz a 2200 Hz).
  * Trava de estabilização térmica (*Warm-up*) de **5 minutos (300 segundos)**: impede alarmes falsos de fogo e gás durante o aquecimento das resistências internas dos sensores.
* **Sinalização Visual e Acústica:**
  * **LED Verde (`GPIO 19`):** Efeito *respirar* contínuo (fade PWM) indicando o ciclo de vida operacional (*heartbeat*).
  * **Buzzer 5V (`GPIO 27`):** Emite um bip curto modulado a cada ciclo de coleta completado com sucesso e bips de inicialização (*boot*). Compatível com buzzers ativos e passivos.
  * **Buzzer 3.3V (`GPIO 12`):** Canal de alta potência dedicado a alarmes (Sirene de Incêndio e Jingle Natalino para detecção de chuva).
* **Gerenciamento de Rede Híbrido & Captive Portal:**
  * **Modo AP Aberto (`atalaia2`):** Gera um ponto de acesso sem senha caso não haja rede configurada ou ocorra falha de conexão.
  * **Captive Portal Integrado:** Intercepta requisições DNS (porta 53) e redireciona automaticamente o navegador de celulares e PCs para o painel de controle ao se conectarem à rede.
  * **Scanner e Configuração Wi-Fi via Web:** Interface integrada para escanear redes disponíveis, selecionar o SSID e inserir a senha sem precisar recompilar o código.
  * **Auto-fallback:** Ao conectar com sucesso a uma rede externa, desliga o AP local. Se a conexão cair ou for resetada, reativa o AP imediatamente.
  * **Resolução mDNS:** Acessível continuamente pelo endereço amigável [http://atalaia2.local](http://atalaia2.local).
* **Persistência Não-Volátil (LittleFS):**
  * **Ring Buffer (Janela Móvel de 8 Horas):** Capacidade para 5.760 registros binários indexados (`/atalaia_buffer.bin`), totalizando ~115,2 kB.
  * **Metadados Operacionais (`/atalaia_meta.bin`):** Preserva a contagem cumulativa perpétua de coletas e a posição atual da cabeça do buffer após reboots e quedas de energia.
* **API REST JSON:**
  * Endpoint `/api/current` serializado para consumo de pipelines externos e dashboards (ex.: scripts Python, Home Assistant).

---

## 🛠️ Pinagem e Conexões (ESP32)

| Identificador | GPIO Físico | Tipo | Função / Conexão | Observações Elétricas |
| :--- | :--- | :--- | :--- | :--- |
| **D4** | GPIO 4 | Entrada Digital | Pino DATA do DHT (11 / 22) | PULLUP interno/externo habilitado |
| **D12** | GPIO 12 | Saída PWM | Buzzer 3.3V (Alarmes de Emergência) | Sirene Policial e Jingle de Chuva |
| **D19** | GPIO 19 | Saída PWM | LED Verde (Heartbeat) | Modulação LEDC, 5 kHz, resolução de 8 bits |
| **D27** | GPIO 27 | Saída PWM | Buzzer 5V (Bip Coleta / Boot) | Modulação em 2.7 kHz (ativo/passivo) |
| **D32** | GPIO 32 | Entrada Analógica | Sensor de Chuva (AO) | Atenuação ADC configurada para 11 dB |
| **D33** | GPIO 33 | Entrada Digital | Sensor IR de Chama (DO) | PULLUP interno, `LOW` = Fogo detectado |
| **D34** | GPIO 34 | Entrada Analógica | Sensor de Gás MQ-135 (AO) | Pino analógico exclusivo de entrada |

### Alimentação e Referências:
* **GND:** Barramento comum interligando o ESP32 a todos os sensores, buzzers e LED.
* **VIN (5V):** Barramento de alimentação para o módulo MQ-135 e circuito do Buzzer de 5V.
* **3.3V:** Barramento de alimentação para DHT, Sensor IR de Chama e Sensor de Chuva.

---

## 📐 Estruturas de Dados (LittleFS)

### Registro Individual (`SensorData`)
Armazenado em binário puro (alinhamento de 24 bytes com padding estruturado):
```cpp
struct __attribute__((packed)) SensorData {
  float    temperature;    // 4 bytes: Temperatura em graus Celsius
  float    humidity;       // 4 bytes: Umidade relativa do ar (%)
  uint32_t uptime_sec;     // 4 bytes: Tempo de atividade da placa em segundos
  uint32_t session_id;     // 4 bytes: ID único gerado a cada boot
  int32_t  mq135_raw;      // 4 bytes: Leitura analógica bruta do sensor MQ-135 (0-4095)
  uint8_t  rain_detected;  // 1 byte:  1 = Chuva detectada, 0 = Seco
  uint8_t  flame_detected; // 1 byte:  1 = Fogo detectado, 0 = Seguro
  uint8_t  padding[2];     // 2 bytes: Alinhamento de memória
};