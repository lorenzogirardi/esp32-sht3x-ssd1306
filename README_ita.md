# ESP32 SHT3x + SSD1306 Weather Station

Stazione meteo indoor con ESP32, sensore temperatura/umidita SHT3x e display OLED SSD1306. Invia dati a InfluxDB via HTTP.

<p align="center">
  <a href="https://res.cloudinary.com/ethzero/image/upload/ai/esp32-sht3x-ssd1306/sensor_full_working.png">
    <img src="https://res.cloudinary.com/ethzero/image/upload/ai/esp32-sht3x-ssd1306/sensor_full_working.png" width="300" alt="Sensore funzionante">
  </a>
  <a href="https://res.cloudinary.com/ethzero/image/upload/ai/esp32-sht3x-ssd1306/sensor_forced_humidity.png">
    <img src="https://res.cloudinary.com/ethzero/image/upload/ai/esp32-sht3x-ssd1306/sensor_forced_humidity.png" width="300" alt="Sensore umidita forzata">
  </a>
</p>

## Funzionalita

- Lettura temperatura e umidita (SHT3x, high repeatability)
- Calcolo **Dew Point** (punto di rugiada)
- Calcolo **Heat Index** (temperatura percepita)
- Rilevamento **Comfort Zone**
- Visualizzazione su display OLED 128x32
- Invio metriche a InfluxDB v1 (line protocol)
- **Metriche Sistema ESP32** (heap, RSSI, uptime, ecc.)
- Reconnessione WiFi automatica

---

## Hardware Richiesto

| Componente | Modello | Indirizzo I2C |
|------------|---------|---------------|
| Microcontrollore | ESP32 DevKit | - |
| Sensore Temp/Hum | SHT3x (SHT30/SHT31/SHT35) | 0x44 |
| Display OLED | SSD1306 128x32 | 0x3C |

---

## Schema Collegamenti

```
                    ESP32 DevKit
                   +-----------+
                   |           |
              3V3 -|*  3V3     |- GND
                   |           |
    SHT3x/OLED VCC-|*  3V3     |- GND -- SHT3x/OLED GND
                   |           |
                   |    GPIO22 |-*-----> SCL (SHT3x + OLED)
                   |           |
                   |    GPIO21 |-*-----> SDA (SHT3x + OLED)
                   |           |
                   |    USB    |
                   +-----*-----+
                         |
                      PC/Power
```

### Connessioni I2C (Bus Condiviso)

```
ESP32                    SHT3x                 SSD1306 OLED
+------+                +------+              +----------+
|      |                |      |              |          |
| 3V3  |-------+--------|  VCC |--------------|  VCC     |
|      |       |        |      |              |          |
| GND  |-------+--------|  GND |--------------|  GND     |
|      |       |        |      |              |          |
|GPIO21|---SDA-+--------|  SDA |--------------|  SDA     |
|      |                |      |              |          |
|GPIO22|---SCL----------|  SCL |--------------|  SCL     |
|      |                |      |              |          |
+------+                +------+              +----------+
                        (0x44)                 (0x3C)
```

---

## Architettura Software

```
+------------------------------------------------------------------+
|                          main.cpp                                 |
+------------------------------------------------------------------+
|                                                                   |
|  +---------+    +---------------------------------------------+   |
|  | setup() |--->| Init: Wire, OLED, SHT3x, WiFi               |   |
|  +---------+    +---------------------------------------------+   |
|                                                                   |
|  +---------+    +---------------------------------------------+   |
|  | loop()  |--->| Non-blocking event loop (millis based)      |   |
|  +----+----+    +---------------------------------------------+   |
|       |                                                           |
|       v                                                           |
|  +---------------------------------------------------------------+|
|  |                    Event Handlers                              ||
|  +---------------------------------------------------------------+|
|  |                                                                ||
|  |  [ogni 5s]        [ogni 60s]           [ogni 60s]             ||
|  |  WiFi Check   -->  Read Sensor      -->  Send InfluxDB        ||
|  |  & Reconnect       & Update OLED         HTTP POST            ||
|  |                                                                ||
|  +---------------------------------------------------------------+|
|                                                                   |
+------------------------------------------------------------------+
```

---

## Flusso Dati

```
+---------+      +-------------+      +--------------+
|  SHT3x  |----->|   ESP32     |----->|   InfluxDB   |
| Sensor  | I2C  |  Processing | HTTP |   Database   |
+---------+      +------+------+      +--------------+
                        |
                        | I2C
                        v
                 +--------------+
                 | SSD1306 OLED |
                 |   Display    |
                 +--------------+
```

### Elaborazione Dati

```
Sensore SHT3x
     |
     v
+---------+     +-------------+     +-------------+
|  Temp   |---->| calcDewPoint|---->|  Dew Point  |
|  (C)    |     |   Magnus    |     |    (C)      |
+---------+     +-------------+     +-------------+
     |
     |          +-------------+     +-------------+
     +--------->|calcHeatIndex|---->| Heat Index  |
     |          |    NOAA     |     |    (C)      |
     v          +-------------+     +-------------+
+---------+
|Humidity |     +-------------+     +-------------+
|  (%)    |---->|getComfortZon|---->| OK/Cold/Hot |
+---------+     |   20-26C    |     | Dry/Humid   |
                |   30-60%    |     +-------------+
                +-------------+
```

---

## Layout Display OLED

```
128 pixels
<-------------------------------------------->
+--------------------------------------------+ ^
|    /-----\    T:22.5    H:55%              | |
|   /       \                                | | 32
|  +---------+  D:13.2    I:22.5             | | pixels
|  | []   [] |                               | |
|  |   |||   |  [OK]                         | v
+--------------------------------------------+

Legenda:
  T: Temperatura (C)
  H: Umidita (%)
  D: Dew Point (C)
  I: Heat Index (C)
  [OK]: Comfort Zone Status
```

---

## Comfort Zone

| Stato | Temperatura | Umidita | Significato |
|-------|-------------|---------|-------------|
| OK | 20-26 C | 30-60% | Comfort ideale |
| Cold | < 20 C | 30-60% | Troppo freddo |
| Hot | > 26 C | 30-60% | Troppo caldo |
| Dry | 20-26 C | < 30% | Troppo secco |
| Humid | 20-26 C | > 60% | Troppo umido |
| Cold+Dry | < 20 C | < 30% | Freddo e secco |
| Cold+Hum | < 20 C | > 60% | Freddo e umido |
| Hot+Dry | > 26 C | < 30% | Caldo e secco |
| Hot+Hum | > 26 C | > 60% | Caldo e umido |

---

## Integrazione InfluxDB

### Formato Line Protocol

```
sensor_data,host=esp32,comfort=OK temp=22.5,humidity=55.0,dewpoint=13.2,heatindex=22.5
+----+----+ +-------+----------+ +---------------------+---------------------------+
     |              |                                   |
 Measurement      Tags                               Fields
```

### Query Esempio (InfluxQL)

```sql
-- Ultime 24 ore
SELECT mean(temp), mean(humidity) FROM sensor_data
WHERE time > now() - 24h
GROUP BY time(1h)

-- Filtra per comfort zone
SELECT * FROM sensor_data WHERE comfort = 'Hot+Hum'
```

### Metriche Sistema ESP32

Un measurement separato `esp32_stats` viene inviato con le metriche di salute della board:

```
esp32_stats,host=esp32 free_heap=45000i,min_free_heap=32000i,heap_size=327680i,rssi=-65i,uptime=3600i,cpu_freq=240i,flash_size=4194304i,sketch_size=957161i,free_sketch=352256i
```

| Campo | Descrizione |
|-------|-------------|
| `free_heap` | RAM disponibile (bytes) |
| `min_free_heap` | RAM minima mai raggiunta (bytes) |
| `heap_size` | RAM totale (bytes) |
| `rssi` | Potenza segnale WiFi (dBm) |
| `uptime` | Secondi dall'accensione |
| `cpu_freq` | Frequenza CPU (MHz) |
| `flash_size` | Memoria flash totale (bytes) |
| `sketch_size` | Dimensione firmware (bytes) |
| `free_sketch` | Spazio firmware disponibile (bytes) |

Query esempio:
```sql
-- Monitorare uso heap nel tempo
SELECT mean(free_heap), min(min_free_heap) FROM esp32_stats
WHERE time > now() - 24h
GROUP BY time(1h)
```

### Dashboard Grafana

Una dashboard pronta e inclusa in `grafana-dashboard.json`.

<p align="center">
  <a href="https://res.cloudinary.com/ethzero/image/upload/ai/esp32-sht3x-ssd1306/grafana-esp32-weather-station-esp32.png">
    <img src="https://res.cloudinary.com/ethzero/image/upload/ai/esp32-sht3x-ssd1306/grafana-esp32-weather-station-esp32.png" width="600" alt="Dashboard Grafana">
  </a>
</p>

**Pannelli inclusi:**
- Temperatura / Dew Point / Heat Index (grafico + stats)
- Umidita (grafico + stat)
- Comfort Zone (stato con colori)
- Potenza segnale WiFi (grafico + stat)
- Uptime
- Frequenza CPU
- Memoria Heap (grafico + gauge)
- Dimensione Flash/Sketch

**Per importare:**
1. Vai in Grafana → **Dashboards → Import**
2. Carica `grafana-dashboard.json` o incolla il contenuto
3. Seleziona il datasource InfluxDB
4. Clicca **Import**

---

## Configurazione

Copia `src/config.h.example` in `src/config.h` e modifica:

```cpp
// WiFi
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASS = "YOUR_PASSWORD";

// InfluxDB v1
const char* INFLUXDB_URL = "http://192.168.1.100:8086/write?db=sensors";
```

Impostazioni timing in `src/main.cpp`:

```cpp
const unsigned long READ_INTERVAL = 60000;   // Lettura ogni 60s
const unsigned long SEND_INTERVAL = 60000;   // Invio ogni 60s
```

---

## Build & Upload

### Requisiti
- PlatformIO CLI o VS Code + PlatformIO IDE

### Comandi

```bash
# Compila
pio run

# Upload su ESP32
pio run -t upload

# Monitor seriale
pio device monitor
```

### Output Seriale Atteso

```
Connecting WiFi.... OK
192.168.1.50
T:22.5 H:55.0 D:13.2 HI:22.5 [OK]
Sending: sensor_data,host=esp32,comfort=OK temp=22.5,humidity=55.0,dewpoint=13.2,heatindex=22.5
HTTP response: 204
```

---

## Troubleshooting

| Problema | Causa | Soluzione |
|----------|-------|-----------|
| `SSD1306 init failed` | OLED non connesso/indirizzo errato | Verifica cablaggio, prova 0x3D |
| `SHT3x error: X` | Sensore non risponde | Verifica I2C, controlla indirizzo |
| `WiFi Failed` | Credenziali errate/segnale debole | Verifica SSID/password |
| `HTTP error` | InfluxDB non raggiungibile | Verifica URL e rete |

### Scansione I2C

Per verificare dispositivi connessi, usa questo sketch:

```cpp
#include <Wire.h>
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("Found: 0x%02X\n", addr);
    }
  }
}
void loop() {}
```

---

## Struttura Progetto

```
esp32-sht3x-ssd1306/
├── platformio.ini          # Configurazione PlatformIO
├── README.md               # Documentazione (English)
├── README_ita.md           # Documentazione (Italiano)
├── grafana-dashboard.json  # Template dashboard Grafana
└── src/
    ├── config.h            # Credenziali (non tracciato)
    ├── config.h.example    # Template credenziali
    └── main.cpp            # Codice sorgente
```

---

## Resistenze Pull-up I2C

Il bus I2C richiede resistenze pull-up sulle linee SDA e SCL. Nella maggior parte dei casi, **non serve aggiungere resistenze esterne** perche:

1. L'ESP32 ha pull-up interni (~45kΩ) abilitati via software
2. La maggior parte dei moduli SHT3x e SSD1306 hanno pull-up sulla loro PCB

### Quando Aggiungere Pull-up Esterni

Aggiungi resistenze da 4.7kΩ se riscontri:
- Errori di lettura intermittenti
- Fallimenti di comunicazione
- Cavi lunghi (>20cm)
- Piu dispositivi sul bus

### Schema di Collegamento

```
              3.3V
               │
          ┌────┴────┐
          │         │
        4.7kΩ     4.7kΩ
          │         │
          │         │
GPIO21 ───┴─────────│───── SDA (sensori)
                    │
GPIO22 ─────────────┴───── SCL (sensori)
```

Ogni resistenza collega la linea dati (SDA o SCL) a 3.3V. Posizionale in qualsiasi punto del circuito tra 3.3V e la rispettiva linea.

---

## Licenza

MIT License
