# Project Context for Claude

## Project Overview
ESP32-based indoor weather station with SHT3x temperature/humidity sensor and SSD1306 OLED display. Sends metrics to InfluxDB v1.

## Current Status: WORKING
- Firmware compiled and uploaded successfully
- WiFi connected to network
- Sensor reading and display working
- InfluxDB data logging active

## Hardware Setup
- **Board**: ESP32 DevKit (MAC: c8:f0:9e:f8:aa:a8)
- **Sensor**: SHT3x at I2C address 0x44
- **Display**: SSD1306 128x32 OLED at I2C address 0x3C
- **I2C Pins**: SDA=GPIO21, SCL=GPIO22
- **Serial Port**: /dev/cu.usbserial-130

## Configuration
Credentials stored in `src/config.h` (git-ignored):
- WiFi SSID/password
- InfluxDB URL (v1 write endpoint)

Template available in `src/config.h.example`

## Key Files
| File | Purpose |
|------|---------|
| `src/main.cpp` | Main firmware code |
| `src/config.h` | Credentials (local only) |
| `src/config.h.example` | Credentials template |
| `platformio.ini` | PlatformIO build config |
| `grafana-dashboard.json` | Grafana dashboard template |

## Build Commands
```bash
pio run              # Build
pio run -t upload    # Upload to ESP32
pio device monitor   # Serial monitor (run in separate terminal)
```

## InfluxDB Schema
```
Measurement: sensor_data
Tags: host=esp32, comfort=<status>
Fields: temp, humidity, dewpoint, heatindex

Measurement: esp32_stats
Tags: host=esp32
Fields: free_heap, min_free_heap, heap_size, rssi, uptime, cpu_freq, flash_size, sketch_size, free_sketch
```

## Features Implemented
- [x] SHT3x sensor reading (temp, humidity)
- [x] Dew point calculation (Magnus formula)
- [x] Heat index calculation (NOAA formula)
- [x] Comfort zone detection
- [x] OLED display with house icon
- [x] WiFi with auto-reconnect
- [x] InfluxDB v1 HTTP POST
- [x] Non-blocking loop (millis-based)
- [x] ESP32 system metrics (heap, RSSI, uptime, CPU freq, flash)

## Potential Future Enhancements
- [ ] Add second sensor for outdoor readings
- [ ] Web interface for configuration
- [ ] MQTT support
- [ ] Battery operation with deep sleep
- [ ] Historical data on display
- [ ] OTA firmware updates

## Repository
https://github.com/lorenzogirardi/esp32-sht3x-ssd1306

## Claude Instructions

### On Every Commit
When committing changes to the codebase:
1. **Update README.md** to reflect any new features, changes to existing functionality, or configuration changes
2. **Update CLAUDE.md** to keep the project context current:
   - Update "Features Implemented" checklist
   - Update "InfluxDB Schema" if measurements/fields change
   - Move completed items from "Potential Future Enhancements"
3. Include both documentation updates in the same commit as the code changes
