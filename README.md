# ATALAIA 3

ATALAIA 3 é uma aplicação web para monitoramento, histórico e análise em tempo real de telemetria ambiental e de risco. O projeto combina Flask, Socket.IO, SQLite e processamento de dados para exibir dashboards interativos e sincronizar com uma estação embarcada ESP32.

## Visão geral

A plataforma foi pensada para coletar dados vindos de sensores de temperatura, umidade, gás, chuva e chama, processar métricas finais e apresentar indicadores em um painel web em tempo real. O sistema também suporta sincronização com o buffer binário da estação ESP32, histórico persistido localmente e gestão de registros.

Principais capacidades:

- Coleta de telemetria em tempo real a partir do ESP32
- Dashboard web com gráficos, KPIs e filtros temporais
- Persistência em SQLite para histórico e auditoria
- Sincronização do buffer binário da estação
- Análises derivadas como heat index, dew point, entalpia e VPD
- Detecção de anomalias e tendências
- API REST para integração externa

## Stack tecnológica

- Python 3.11+
- Flask
- Flask-SocketIO
- Gevent
- SQLite
- NumPy, Pandas, scikit-learn
- Requests

## Estrutura do repositório

```text
atalaia3/
├── app/
│   ├── analytics/
│   │   ├── anomaly_engine.py
│   │   ├── bioclimatic.py
│   │   ├── gas_metrics.py
│   │   └── trend_analyzer.py
│   ├── core/
│   │   ├── binary_decoder.py
│   │   ├── data_manager.py
│   │   ├── database.py
│   │   └── esp_client.py
│   ├── routes/
│   │   ├── api.py
│   │   ├── live_ws.py
│   │   └── views.py
│   ├── static/
│   ├── templates/
│   └── __init__.py
├── .env.example
├── config.py
├── README.md
├── requirements.txt
├── run.py
└── atalaia3_history.db
```

## Como executar

1. Crie um ambiente virtual:

```bash
python -m venv .venv
source .venv/bin/activate
```

No Windows:

```powershell
.venv\Scripts\activate
```

2. Instale as dependências:

```bash
pip install -r requirements.txt
```

3. Configure as variáveis de ambiente:

```bash
cp .env.example .env
```

Arquivo de exemplo:

```env
SECRET_KEY=atalaia3_super_secret_matrix_key_2026
ESP32_HOST=http://atalaia2.local
ESP32_FALLBACK_IP=http://192.168.4.1
DATABASE_PATH=atalaia3_history.db
SAMPLE_RATE_SEC=5
```

4. Inicie a aplicação:

```bash
python run.py
```

A aplicação fica disponível em:

- http://localhost:5000

## Funcionalidades do painel

O dashboard principal expõe:

- gráficos de temperatura, umidade e gases
- KPIs derivados de telemetria
- comparação por janela temporal
- indicadores de risco/anomalia
- histórico de amostras e registros
- gestão de registros em interface web

Rotas web principais:

- `/` — painel principal
- `/records` — gestão e auditoria de registros

## API REST

A aplicação expõe endpoints em `/api`:

```text
GET  /api/telemetry/data?start=YYYY-MM-DD&end=YYYY-MM-DD
GET  /api/records/list?page=1&size=20
POST /api/records/delete
POST /api/records/purge_all
POST /api/esp/sync
POST /api/esp/actuator
```

Exemplo de consulta:

```bash
curl "http://localhost:5000/api/telemetry/data?start=2026-01-01&end=2026-01-31"
```

## Fluxo de operação

1. O ESP32 publica dados em um endpoint de telemetria local
2. O cliente HTTP do backend tenta a rota primária e faz fallback para IP local
3. A aplicação enriquece as amostras com métricas derivadas
4. Os dados são persistidos em SQLite
5. O dashboard e o cliente WebSocket são atualizados em tempo real
6. O operador pode sincronizar ou limpar o buffer do módulo embarcado

## Dados e métricas

Os registros armazenados incluem, entre outros campos:

- timestamp
- session_id
- uptime_sec
- temperature
- humidity
- mq135_raw
- rain_detected
- flame_detected
- heat_index
- dew_point
- gas_ppm
- is_anomaly
- source

## Observações importantes

- A aplicação depende de um host ESP32 acessível em `ESP32_HOST` ou no fallback `ESP32_FALLBACK_IP`
- O projeto foi pensado para uso em rede local, com painel de supervisão centralizado
- A sincronização de buffer usa a API do módulo embarcado para importar amostras históricas

## Licença

Este projeto não especifica uma licença explícita no repositório. Verifique a configuração do GitHub antes de reutilizar o código em produção ou distribuí-lo publicamente.

## Contribuição

Sinta-se à vontade para abrir issues e pull requests com melhorias, correções e novos módulos de análise.
