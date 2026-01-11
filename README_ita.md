# ESP32 SHT3x + SSD1306 Weather Station

Stazione meteo indoor con ESP32, sensore temperatura/umidita SHT3x e display OLED SSD1306. Invia dati a InfluxDB via HTTP.

## Funzionalita

- Lettura temperatura e umidita (SHT3x, high repeatability)
- Calcolo **Dew Point** (punto di rugiada)
- Calcolo **Heat Index** (temperatura percepita)
- Rilevamento **Comfort Zone**
- Visualizzazione su display OLED 128x32
- Invio metriche a InfluxDB v1 (line protocol)
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
              3V3 -|●  3V3     |- GND
                   |           |
    SHT3x/OLED VCC-|●  3V3     |- GND -- SHT3x/OLED GND
                   |           |
                   |    GPIO22 |-●-----> SCL (SHT3x + OLED)
                   |           |
                   |    GPIO21 |-●-----> SDA (SHT3x + OLED)
                   |           |
                   |    USB    |
                   +-----●-----+
                         |
                      PC/Power
```

### Connessioni I2C (Bus Condiviso)

```
ESP32                    SHT3x                 SSD1306 OLED
┌──────┐                ┌──────┐              ┌──────────┐
│      │                │      │              │          │
│ 3V3  │───────┬────────│ VCC  │──────────────│ VCC      │
│      │       │        │      │              │          │
│ GND  │───────┼────────│ GND  │──────────────│ GND      │
│      │       │        │      │              │          │
│GPIO21│───SDA─┴────────│ SDA  │──────────────│ SDA      │
│      │                │      │              │          │
│GPIO22│───SCL──────────│ SCL  │──────────────│ SCL      │
│      │                │      │              │          │
└──────┘                └──────┘              └──────────┘
                        (0x44)                 (0x3C)
```

---

## Architettura Software

```
┌─────────────────────────────────────────────────────────────────┐
│                          main.cpp                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────┐    ┌──────────────────────────────────────────┐    │
│  │ setup() │───>│ Init: Wire, OLED, SHT3x, WiFi            │    │
│  └─────────┘    └──────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────┐    ┌──────────────────────────────────────────┐    │
│  │ loop()  │───>│ Non-blocking event loop (millis based)   │    │
│  └────┬────┘    └──────────────────────────────────────────┘    │
│       │                                                          │
│       ▼                                                          │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │                    Event Handlers                           │ │
│  ├────────────────────────────────────────────────────────────┤ │
│  │                                                             │ │
│  │  [ogni 5s]          [ogni 60s]           [ogni 60s]        │ │
│  │  WiFi Check    -->  Read Sensor     -->  Send InfluxDB     │ │
│  │  & Reconnect        & Update OLED        HTTP POST         │ │
│  │                                                             │ │
│  └────────────────────────────────────────────────────────────┘ │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Flusso Dati

```
┌─────────┐      ┌─────────────┐      ┌──────────────┐
│  SHT3x  │─────>│   ESP32     │─────>│   InfluxDB   │
│ Sensor  │ I2C  │  Processing │ HTTP │   Database   │
└─────────┘      └──────┬──────┘      └──────────────┘
                        │
                        │ I2C
                        ▼
                 ┌──────────────┐
                 │ SSD1306 OLED │
                 │   Display    │
                 └──────────────┘
```

### Elaborazione Dati

```
Sensore SHT3x
     │
     ▼
┌─────────┐     ┌─────────────┐     ┌─────────────┐
│  Temp   │────>│ calcDewPoint│────>│  Dew Point  │
│  (C)    │     │   Magnus    │     │    (C)      │
└─────────┘     └─────────────┘     └─────────────┘
     │
     │          ┌─────────────┐     ┌─────────────┐
     └─────────>│calcHeatIndex│────>│ Heat Index  │
     │          │    NOAA     │     │    (C)      │
     ▼          └─────────────┘     └─────────────┘
┌─────────┐
│Humidity │     ┌─────────────┐     ┌─────────────┐
│  (%)    │────>│getComfortZon│────>│ OK/Cold/Hot │
└─────────┘     │   20-26C    │     │ Dry/Humid   │
                │   30-60%    │     └─────────────┘
                └─────────────┘
```

---

## Layout Display OLED

```
128 pixels
◄──────────────────────────────────────────►
┌──────────────────────────────────────────┐ ▲
│    ╱▔▔▔▔▔╲    T:22.5    H:55%            │ │
│   ╱       ╲                              │ │ 32
│  ├─────────┤  D:13.2    I:22.5           │ │ pixels
│  │ □     □ │                             │ │
│  │   ▯▯▯   │  [OK]                       │ ▼
└──────────────────────────────────────────┘

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

## InfluxDB Integration

### Line Protocol Format

```
sensor_data,host=esp32,comfort=OK temp=22.5,humidity=55.0,dewpoint=13.2,heatindex=22.5
└────┬────┘ └───────┬──────────┘ └─────────────────────┬─────────────────────────────┘
     │              │                                   │
 Measurement      Tags                               Fields
```

### Query Esempio (InfluxQL)

```sql
-- Ultimi 24h
SELECT mean(temp), mean(humidity) FROM sensor_data
WHERE time > now() - 24h
GROUP BY time(1h)

-- Filtra per comfort zone
SELECT * FROM sensor_data WHERE comfort = 'Hot+Hum'
```

---

## Configurazione

Modifica in `src/main.cpp`:

```cpp
// WiFi
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASS = "YOUR_PASSWORD";

// InfluxDB v1
const char* INFLUXDB_URL = "http://192.168.1.100:8086/write?db=sensors";

// Timing (ms)
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
├── platformio.ini      # Configurazione PlatformIO
├── README.md           # Questa documentazione
└── src/
    └── main.cpp        # Codice sorgente
```

---

## Licenza

MIT License
