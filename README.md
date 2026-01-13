# ESP32 SHT3x + SSD1306 Weather Station

Indoor weather station with ESP32, SHT3x temperature/humidity sensor and SSD1306 OLED display. Sends data to InfluxDB via HTTP.

<p align="center">
  <a href="https://res.cloudinary.com/ethzero/image/upload/ai/esp32-sht3x-ssd1306/sensor_full_working.png">
    <img src="https://res.cloudinary.com/ethzero/image/upload/ai/esp32-sht3x-ssd1306/sensor_full_working.png" width="300" alt="Sensor working">
  </a>
  <a href="https://res.cloudinary.com/ethzero/image/upload/ai/esp32-sht3x-ssd1306/sensor_forced_humidity.png">
    <img src="https://res.cloudinary.com/ethzero/image/upload/ai/esp32-sht3x-ssd1306/sensor_forced_humidity.png" width="300" alt="Sensor forced humidity">
  </a>
</p>

## Features

- Temperature and humidity reading (SHT3x, high repeatability)
- **Dew Point** calculation
- **Heat Index** calculation (perceived temperature)
- **Comfort Zone** detection
- OLED 128x32 display output
- InfluxDB v1 metrics export (line protocol)
- **ESP32 System Metrics** (heap, RSSI, uptime, etc.)
- Automatic WiFi reconnection

---

## Required Hardware

| Component | Model | I2C Address |
|-----------|-------|-------------|
| Microcontroller | ESP32 DevKit | - |
| Temp/Hum Sensor | SHT3x (SHT30/SHT31/SHT35) | 0x44 |
| OLED Display | SSD1306 128x32 | 0x3C |

---

## Wiring Diagram

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

### I2C Connections (Shared Bus)

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

## Software Architecture

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
|  |  [every 5s]        [every 60s]           [every 60s]          ||
|  |  WiFi Check   -->  Read Sensor      -->  Send InfluxDB        ||
|  |  & Reconnect       & Update OLED         HTTP POST            ||
|  |                                                                ||
|  +---------------------------------------------------------------+|
|                                                                   |
+------------------------------------------------------------------+
```

---

## Data Flow

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

### Data Processing

```
SHT3x Sensor
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
+---------+                           * see note
|Humidity |     +-------------+     +-------------+
|  (%)    |---->|getComfortZon|---->| OK/Cold/Hot |
+---------+     |   20-26C    |     | Dry/Humid   |
                |   30-60%    |     +-------------+
                +-------------+
```

**\* Heat Index Note:** The Heat Index is only calculated when temperature exceeds 26.7°C (80°F). Below this threshold, humidity has negligible effect on perceived temperature, so the actual temperature is returned. This follows the NOAA formula design, which measures heat stress rather than general comfort.

| Temperature | Humidity Effect |
|-------------|-----------------|
| < 27°C | Negligible - humidity doesn't affect perception |
| > 27°C | Significant - high humidity prevents sweat evaporation |

---

## OLED Display Layout

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

Legend:
  T: Temperature (C)
  H: Humidity (%)
  D: Dew Point (C)
  I: Heat Index (C)
  [OK]: Comfort Zone Status
```

---

## Comfort Zone

| Status | Temperature | Humidity | Meaning |
|--------|-------------|----------|---------|
| OK | 20-26 C | 30-60% | Ideal comfort |
| Cold | < 20 C | 30-60% | Too cold |
| Hot | > 26 C | 30-60% | Too hot |
| Dry | 20-26 C | < 30% | Too dry |
| Humid | 20-26 C | > 60% | Too humid |
| Cold+Dry | < 20 C | < 30% | Cold and dry |
| Cold+Hum | < 20 C | > 60% | Cold and humid |
| Hot+Dry | > 26 C | < 30% | Hot and dry |
| Hot+Hum | > 26 C | > 60% | Hot and humid |

---

## InfluxDB Integration

### Line Protocol Format

```
sensor_data,host=esp32,comfort=OK temp=22.5,humidity=55.0,dewpoint=13.2,heatindex=22.5
+----+----+ +-------+----------+ +---------------------+---------------------------+
     |              |                                   |
 Measurement      Tags                               Fields
```

### Example Queries (InfluxQL)

```sql
-- Last 24 hours
SELECT mean(temp), mean(humidity) FROM sensor_data
WHERE time > now() - 24h
GROUP BY time(1h)

-- Filter by comfort zone
SELECT * FROM sensor_data WHERE comfort = 'Hot+Hum'
```

### ESP32 System Metrics

A separate measurement `esp32_stats` is sent with board health metrics:

```
esp32_stats,host=esp32 free_heap=45000i,min_free_heap=32000i,heap_size=327680i,rssi=-65i,uptime=3600i,cpu_freq=240i,flash_size=4194304i,sketch_size=957161i,free_sketch=352256i
```

| Field | Description |
|-------|-------------|
| `free_heap` | Available RAM (bytes) |
| `min_free_heap` | Lowest RAM ever reached (bytes) |
| `heap_size` | Total RAM (bytes) |
| `rssi` | WiFi signal strength (dBm) |
| `uptime` | Seconds since boot |
| `cpu_freq` | CPU frequency (MHz) |
| `flash_size` | Total flash memory (bytes) |
| `sketch_size` | Firmware size (bytes) |
| `free_sketch` | Available firmware space (bytes) |

Example query:
```sql
-- Monitor heap usage over time
SELECT mean(free_heap), min(min_free_heap) FROM esp32_stats
WHERE time > now() - 24h
GROUP BY time(1h)
```

### Grafana Dashboard

A pre-built dashboard is included in `grafana-dashboard.json`.

<p align="center">
  <a href="https://res.cloudinary.com/ethzero/image/upload/ai/esp32-sht3x-ssd1306/grafana-esp32-weather-station-esp32.png?v=2">
    <img src="https://res.cloudinary.com/ethzero/image/upload/ai/esp32-sht3x-ssd1306/grafana-esp32-weather-station-esp32.png?v=2" width="600" alt="Grafana Dashboard">
  </a>
</p>

**Panels included:**
- Temperature / Dew Point / Heat Index (graph + stats)
- Humidity (graph + stat)
- Comfort Zone (color-coded status)
- WiFi Signal strength (graph + stat)
- Uptime
- CPU Frequency
- Heap Memory (graph + gauges)
- Flash/Sketch size

**To import:**
1. Go to Grafana → **Dashboards → Import**
2. Upload `grafana-dashboard.json` or paste its content
3. Select your InfluxDB datasource
4. Click **Import**

---

## Configuration

Copy `src/config.h.example` to `src/config.h` and edit:

```cpp
// WiFi
const char* WIFI_SSID = "YOUR_SSID";
const char* WIFI_PASS = "YOUR_PASSWORD";

// InfluxDB v1
const char* INFLUXDB_URL = "http://192.168.1.100:8086/write?db=sensors";
```

Timing settings in `src/main.cpp`:

```cpp
const unsigned long READ_INTERVAL = 60000;   // Read every 60s
const unsigned long SEND_INTERVAL = 60000;   // Send every 60s
```

---

## Build & Upload

### Requirements
- PlatformIO CLI or VS Code + PlatformIO IDE

### Commands

```bash
# Build
pio run

# Upload to ESP32
pio run -t upload

# Serial monitor
pio device monitor
```

### Expected Serial Output

```
Connecting WiFi.... OK
192.168.1.50
T:22.5 H:55.0 D:13.2 HI:22.5 [OK]
Sending: sensor_data,host=esp32,comfort=OK temp=22.5,humidity=55.0,dewpoint=13.2,heatindex=22.5
HTTP response: 204
```

---

## Troubleshooting

| Problem | Cause | Solution |
|---------|-------|----------|
| `SSD1306 init failed` | OLED not connected/wrong address | Check wiring, try 0x3D |
| `SHT3x error: X` | Sensor not responding | Check I2C, verify address |
| `WiFi Failed` | Wrong credentials/weak signal | Verify SSID/password |
| `HTTP error` | InfluxDB unreachable | Check URL and network |

### I2C Scanner

To check connected devices, use this sketch:

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

## Project Structure

```
esp32-sht3x-ssd1306/
├── platformio.ini          # PlatformIO configuration
├── README.md               # This documentation
├── README_ita.md           # Italian documentation
├── grafana-dashboard.json  # Grafana dashboard template
└── src/
    ├── config.h            # Your credentials (not tracked)
    ├── config.h.example    # Template for credentials
    └── main.cpp            # Source code
```

---

## I2C Pull-up Resistors

The I2C bus requires pull-up resistors on SDA and SCL lines. In most cases, **you don't need to add external resistors** because:

1. ESP32 has internal pull-ups (~45kΩ) enabled by software
2. Most SHT3x and SSD1306 modules have pull-ups on their PCB

### When to Add External Pull-ups

Add 4.7kΩ resistors if you experience:
- Intermittent reading errors
- Communication failures
- Long wires (>20cm)
- Multiple devices on the bus

### Wiring Diagram

```
              3.3V
               │
          ┌────┴────┐
          │         │
        4.7kΩ     4.7kΩ
          │         │
          │         │
GPIO21 ───┴─────────│───── SDA (sensors)
                    │
GPIO22 ─────────────┴───── SCL (sensors)
```

Each resistor connects the data line (SDA or SCL) to 3.3V. Place them anywhere on the circuit between 3.3V and the respective line.

---

## License 

MIT License
