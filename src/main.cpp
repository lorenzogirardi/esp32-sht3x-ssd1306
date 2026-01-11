#include <Arduino.h>
#include <math.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SensirionI2cSht3x.h>
#include "config.h"

// Function prototypes
void drawHouse(int x, int y);
void updateDisplay(float temp, float hum, float dew, float hi, const char* comfort);
void displayError();
void sendToInfluxDB(float temp, float hum, float dew, float hi, const char* comfort);

// I2C
#define SDA_PIN 21
#define SCL_PIN 22

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_ADDR 0x3C

// SHT3x
#define SHT3X_ADDR 0x44

// Timing (ms)
const unsigned long READ_INTERVAL = 60000;       // Lettura sensore + display (1 minuto)
const unsigned long SEND_INTERVAL = 60000;       // Invio a InfluxDB (1 minuto)
const unsigned long WIFI_RETRY_INTERVAL = 5000;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
SensirionI2cSht3x sht3x;

unsigned long lastReadTime = 0;
unsigned long lastSendTime = 0;
unsigned long lastWifiRetry = 0;
float lastTemp = 0;
float lastHum = 0;
float lastDew = 0;
float lastHeatIdx = 0;
const char* lastComfort = "---";

// Calcolo Dew Point (Magnus formula)
float calcDewPoint(float temp, float hum) {
  const float b = 17.62;
  const float c = 243.12;
  float gamma = log(hum / 100.0) + (b * temp) / (c + temp);
  return (c * gamma) / (b - gamma);
}

// Calcolo Heat Index (NOAA formula)
float calcHeatIndex(float tempC, float hum) {
  float T = tempC * 1.8 + 32.0;  // Celsius to Fahrenheit

  if (T < 80.0) return tempC;  // No heat index under 26.7C

  float HI = -42.379 + 2.04901523*T + 10.14333127*hum
             - 0.22475541*T*hum - 0.00683783*T*T
             - 0.05481717*hum*hum + 0.00122874*T*T*hum
             + 0.00085282*T*hum*hum - 0.00000199*T*T*hum*hum;

  return (HI - 32.0) / 1.8;  // Back to Celsius
}

// Comfort zone: T 20-26C, H 30-60%
const char* getComfortZone(float temp, float hum) {
  bool tempOk = (temp >= 20.0 && temp <= 26.0);
  bool humOk = (hum >= 30.0 && hum <= 60.0);

  if (tempOk && humOk) return "OK";
  if (temp < 20.0 && humOk) return "Cold";
  if (temp > 26.0 && humOk) return "Hot";
  if (tempOk && hum < 30.0) return "Dry";
  if (tempOk && hum > 60.0) return "Humid";
  if (temp < 20.0 && hum < 30.0) return "Cold+Dry";
  if (temp < 20.0 && hum > 60.0) return "Cold+Hum";
  if (temp > 26.0 && hum < 30.0) return "Hot+Dry";
  return "Hot+Hum";
}

void setup() {
  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);

  // Init OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 init failed");
    while (1);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Initializing...");
  display.display();

  // Init SHT3x
  sht3x.begin(Wire, SHT3X_ADDR);
  sht3x.stopMeasurement();
  sht3x.softReset();
  delay(100);

  // Connect WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting WiFi");

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" OK");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" Failed");
  }
}

void loop() {
  unsigned long now = millis();

  // WiFi reconnection
  if (WiFi.status() != WL_CONNECTED && now - lastWifiRetry >= WIFI_RETRY_INTERVAL) {
    lastWifiRetry = now;
    Serial.println("WiFi reconnecting...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }

  // Read sensor + update display
  if (now - lastReadTime >= READ_INTERVAL) {
    lastReadTime = now;

    float temp, hum;
    uint16_t error = sht3x.measureSingleShot(REPEATABILITY_HIGH, false, temp, hum);

    if (error) {
      Serial.print("SHT3x error: ");
      Serial.println(error);
      displayError();
      return;
    }

    lastTemp = temp;
    lastHum = hum;
    lastDew = calcDewPoint(temp, hum);
    lastHeatIdx = calcHeatIndex(temp, hum);
    lastComfort = getComfortZone(temp, hum);

    Serial.print("T:");
    Serial.print(temp, 1);
    Serial.print(" H:");
    Serial.print(hum, 1);
    Serial.print(" D:");
    Serial.print(lastDew, 1);
    Serial.print(" HI:");
    Serial.print(lastHeatIdx, 1);
    Serial.print(" [");
    Serial.print(lastComfort);
    Serial.println("]");

    updateDisplay(temp, hum, lastDew, lastHeatIdx, lastComfort);
  }

  // Send to InfluxDB
  if (now - lastSendTime >= SEND_INTERVAL) {
    lastSendTime = now;
    sendToInfluxDB(lastTemp, lastHum, lastDew, lastHeatIdx, lastComfort);
  }
}

void drawHouse(int x, int y) {
  // Tetto spiovente largo (trapezio)
  display.fillTriangle(x+9, y, x-2, y+8, x+20, y+8, SSD1306_WHITE);
  // Linea sotto tetto
  display.drawLine(x, y+8, x+18, y+8, SSD1306_WHITE);
  // Corpo casa
  display.fillRect(x, y+9, 18, 12, SSD1306_WHITE);
  // Finestra sinistra
  display.fillRect(x+2, y+11, 4, 4, SSD1306_BLACK);
  // Finestra destra
  display.fillRect(x+12, y+11, 4, 4, SSD1306_BLACK);
  // Porta centrale
  display.fillRect(x+7, y+14, 4, 7, SSD1306_BLACK);
}

void updateDisplay(float temp, float hum, float dew, float hi, const char* comfort) {
  display.clearDisplay();

  // Disegna casa a sinistra
  drawHouse(2, 5);

  // Riga 1: Temp e Humidity
  display.setCursor(26, 0);
  display.print("T:");
  display.print(temp, 1);
  display.setCursor(74, 0);
  display.print("H:");
  display.print(hum, 0);
  display.print("%");

  // Riga 2: Dew e HeatIndex
  display.setCursor(26, 12);
  display.print("D:");
  display.print(dew, 1);
  display.setCursor(74, 12);
  display.print("I:");
  display.print(hi, 1);

  // Riga 3: Comfort zone
  display.setCursor(26, 24);
  display.print("[");
  display.print(comfort);
  display.print("]");

  display.display();
}

void displayError() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Sensor Error");
  display.display();
}

void sendToInfluxDB(float temp, float hum, float dew, float hi, const char* comfort) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skip send");
    return;
  }

  HTTPClient http;
  http.begin(INFLUXDB_URL);
  http.addHeader("Content-Type", "text/plain");

  // Line protocol: comfort as tag, rest as fields
  char payload[180];
  snprintf(payload, sizeof(payload),
           "sensor_data,host=esp32,comfort=%s temp=%.1f,humidity=%.1f,dewpoint=%.1f,heatindex=%.1f",
           comfort, temp, hum, dew, hi);

  Serial.print("Sending: ");
  Serial.println(payload);

  int httpCode = http.POST(payload);

  if (httpCode > 0) {
    Serial.print("HTTP response: ");
    Serial.println(httpCode);
  } else {
    Serial.print("HTTP error: ");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();
}
