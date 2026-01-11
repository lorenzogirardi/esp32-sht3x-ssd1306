# ESP32 SHT3x + SSD1306 Weather Station

Indoor weather station with ESP32, SHT3x temperature/humidity sensor and SSD1306 OLED display. Sends data to InfluxDB via HTTP.

## Features

- Temperature and humidity reading (SHT3x, high repeatability)
- **Dew Point** calculation
- **Heat Index** calculation (perceived temperature)
- **Comfort Zone** detection
- OLED 128x32 display output
- InfluxDB v1 metrics export (line protocol)
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
+---------+
|Humidity |     +-------------+     +-------------+
|  (%)    |---->|getComfortZon|---->| OK/Cold/Hot |
+---------+     |   20-26C    |     | Dry/Humid   |
                |   30-60%    |     +-------------+
                +-------------+
```

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
├── platformio.ini       # PlatformIO configuration
├── README.md            # This documentation
├── README_ita.md        # Italian documentation
└── src/
    ├── config.h         # Your credentials (not tracked)
    ├── config.h.example # Template for credentials
    └── main.cpp         # Source code
```

---

## License

MIT License
