# ESP8266 Temperature Monitor v2.1.0

An advanced ESP8266-based IoT temperature and humidity monitoring system with enhanced telemetry, energy efficiency, and comprehensive error handling.

## Version 2.1.0 Features

### 🚀 Enhanced Functionality
- **Advanced Environmental Metrics**: Heat index, dew point, absolute humidity, and comfort level assessment
- **Intelligent Data Transmission**: Only sends data on significant changes or at regular intervals
- **24-Hour Statistics**: Rolling averages, min/max tracking over 24-hour periods
- **Deep Sleep Mode**: Ultra-low power consumption with 5-minute wake intervals
- **Comprehensive Error Handling**: Retry logic, connection recovery, and error tracking
- **Performance Monitoring**: Real-time metrics for WiFi, sensor, and API response times

### 📊 Telemetry Levels
- **Basic Telemetry**: Core sensor data and metadata (sent on significant changes)
- **Full Telemetry**: Complete system diagnostics (sent every 12th reading/hourly)

### 🔋 Power Management
- Deep sleep between readings for battery operation
- LED status indicators for operation feedback
- VCC voltage monitoring

## Hardware Requirements

- ESP8266 development board (NodeMCU, Wemos D1 Mini, etc.)
- DHT22 temperature/humidity sensor
- Jumper wires
- Optional: Battery pack for portable operation

## Wiring

Connect the DHT22 sensor to the ESP8266:
- VCC → 3.3V
- GND → GND  
- DATA → GPIO2 (D4 on NodeMCU)

## Arduino IDE Setup

1. Install the ESP8266 board package in Arduino IDE
2. Install required libraries:
   - ESP8266WiFi (included with ESP8266 package)
   - ESP8266HTTPClient (included with ESP8266 package)
   - WiFiClientSecure (included with ESP8266 package)
   - DHT sensor library by Adafruit
   - ArduinoJson by Benoit Blanchon

## Configuration

### 1. WiFi Credentials
Update WiFi settings in `esp8266_temp_monitor.ino`:
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

### 2. API Endpoint
Configure your API endpoint:
```cpp
const char* apiEndpoint = "YOUR_API_ENDPOINT";
```

### 3. Adjustable Parameters
- `SLEEP_INTERVAL`: Deep sleep duration (default: 5 minutes)
- `TEMP_CHANGE_THRESHOLD`: Temperature change threshold for transmission (default: 5°F)
- `HUMIDITY_CHANGE_THRESHOLD`: Humidity change threshold for transmission (default: 10%)
- `FULL_TELEMETRY_INTERVAL`: Full telemetry transmission interval (default: every 12 readings)

## API Specification

The OpenAPI 3.0 specification for the sensor data API is provided in `sensor-api.yaml`. The system supports both basic and full telemetry modes.

## Data Formats

### Basic Telemetry (Sent on significant changes)
```json
{
  "sensor_data": {
    "temperature": 72.3,
    "temperature_unit": "F",
    "humidity": 45.2,
    "heat_index": 72.1,
    "dew_point": 50.8,
    "absolute_humidity": 7.9,
    "comfort_level": "comfortable"
  },
  "metadata": {
    "timestamp": 1640995200,
    "timestamp_iso": "2022-01-01T12:00:00Z",
    "local_time": "2022-01-01 07:00:00 CDT",
    "device_id": "AA:BB:CC:DD:EE:FF",
    "sensor_type": "DHT22",
    "firmware_version": "2.1.0",
    "transmission_type": "basic"
  }
}
```

### Full Telemetry (Sent hourly)
Includes all basic data plus:
- **Device Information**: Chip details, memory usage, SDK version
- **Power Management**: VCC voltage, sleep settings
- **Connectivity**: WiFi quality, signal strength, network details
- **Performance Metrics**: Timing data, heap usage, CPU frequency
- **Statistics**: Success rates, error counts, 24-hour trends
- **Diagnostics**: Recent error history

## System Features

### 🛡️ Reliability
- **Multiple Reading Attempts**: Up to 3 attempts for sensor readings
- **Connection Recovery**: Automatic WiFi reconnection
- **Data Validation**: Range checking and sanity validation
- **Persistent Storage**: EEPROM storage for statistics and error history

### 📈 Performance Monitoring
- WiFi connection time tracking
- Sensor read time monitoring
- API response time measurement
- Memory usage diagnostics

### 🔧 Error Handling
- Comprehensive error categorization
- Error history tracking (last 5 errors)
- Automatic retry mechanisms
- Graceful degradation on failures

### 💡 Status Indicators
- **LED Patterns**:
  - 2 quick blinks: Successful operation
  - 5 fast blinks: Error condition
  - Solid during operation

## Monitoring and Diagnostics

The system provides extensive monitoring capabilities:

- **Real-time Metrics**: Temperature, humidity, heat index, dew point
- **Environmental Assessment**: Comfort level classification
- **24-Hour Trends**: Rolling statistics and extremes
- **System Health**: Success rates, error frequencies
- **Performance Data**: Timing metrics and resource usage

## Version History

- **v2.1.0**: Enhanced telemetry edition with advanced environmental metrics, deep sleep, and comprehensive monitoring
- **v2.0.0**: API version 2.0 with enhanced payload structure
- **v1.x**: Basic temperature and humidity monitoring

## License

This project is open source and available under standard licensing terms.