# ESP8266 Temperature Monitor

This project contains code for an ESP8266-based temperature and humidity monitoring system that posts data to a REST API every 5 minutes.

## Hardware Requirements

- ESP8266 development board (NodeMCU, Wemos D1 Mini, etc.)
- DHT22 temperature/humidity sensor
- Jumper wires

## Wiring

Connect the DHT22 sensor to the ESP8266:
- VCC → 3.3V
- GND → GND  
- DATA → GPIO2 (D4 on NodeMCU)

## Arduino IDE Setup

1. Install the ESP8266 board package in Arduino IDE
2. Install required libraries:
   - ESP8266WiFi (included with ESP8266 package)
   - DHT sensor library by Adafruit
   - ArduinoJson by Benoit Blanchon

## Configuration

1. Update WiFi credentials in `esp8266_temp_monitor.ino`:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```

2. Update API endpoint:
   ```cpp
   const char* apiEndpoint = "https://your-api-domain.com/api/sensor-data";
   ```

## API Specification

The OpenAPI 3.0 specification for the sensor data API is provided in `sensor-api.yaml`. The API expects JSON POST requests with temperature, humidity, timestamp, and device ID.

## Data Format

The ESP8266 sends data in this JSON format:
```json
{
  "temperature": 23.5,
  "humidity": 65.2,
  "timestamp": 1640995200,
  "device_id": "AA:BB:CC:DD:EE:FF"
}
```