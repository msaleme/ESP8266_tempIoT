#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <time.h>
#include <EEPROM.h>

#define DHT_PIN 2
#define DHT_TYPE DHT22
#define LED_PIN LED_BUILTIN
#define EEPROM_SIZE 512

// Configuration
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* apiEndpoint = "YOUR_API_ENDPOINT";
const char* firmwareVersion = "2.1.0";

// Sleep settings for energy efficiency
const unsigned long SLEEP_INTERVAL = 5 * 60 * 1000000; // 5 minutes in microseconds
const unsigned long WIFI_TIMEOUT = 10000; // 10 seconds
const unsigned long API_TIMEOUT = 15000;  // 15 seconds

// Data validation thresholds
const float MIN_TEMP_F = -40.0;
const float MAX_TEMP_F = 176.0;
const float MIN_HUMIDITY = 0.0;
const float MAX_HUMIDITY = 100.0;
const float TEMP_CHANGE_THRESHOLD = 5.0; // Degrees F
const float HUMIDITY_CHANGE_THRESHOLD = 10.0; // Percent

// Retry configuration
const int MAX_RETRIES = 3;
const unsigned long RETRY_DELAY = 2000;

// Enhanced telemetry intervals
const int FULL_TELEMETRY_INTERVAL = 12; // Send full telemetry every 12th reading (1 hour)

// EEPROM addresses for data persistence
const int ADDR_LAST_TEMP = 0;
const int ADDR_LAST_HUMIDITY = 4;
const int ADDR_FAILURE_COUNT = 8;
const int ADDR_TOTAL_READINGS = 12;
const int ADDR_24H_STATS = 16; // 24-hour statistics block (48 bytes)
const int ADDR_ERROR_HISTORY = 64; // Error history (20 bytes)

DHT dht(DHT_PIN, DHT_TYPE);
WiFiClientSecure wifiClient;
HTTPClient http;

// Enhanced sensor data structure
struct SensorData {
  float temperature;
  float humidity;
  float heatIndex;
  float dewPoint;
  float absoluteHumidity;
  time_t timestamp;
  String comfortLevel;
};

// Statistics tracking
struct SensorStats {
  float lastTemperature;
  float lastHumidity;
  uint32_t failureCount;
  uint32_t totalReadings;
  uint32_t successfulTransmissions;
  uint32_t failedTransmissions;
  uint32_t wifiDisconnects;
  uint32_t apiErrors;
};

// 24-hour rolling statistics
struct DailyStats {
  float avgTemp24h;
  float minTemp24h;
  float maxTemp24h;
  float avgHumidity24h;
  float minHumidity24h;
  float maxHumidity24h;
  uint16_t readingCount24h;
};

// Performance metrics
struct PerformanceMetrics {
  unsigned long wifiConnectTime;
  unsigned long sensorReadTime;
  unsigned long apiResponseTime;
  unsigned long loopExecutionTime;
  unsigned long startTime;
};

// Error tracking
struct ErrorHistory {
  char lastErrors[5][16]; // Last 5 errors
  uint8_t errorIndex;
};

SensorStats stats;
DailyStats dailyStats;
ErrorHistory errorHistory;
PerformanceMetrics perf;

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP8266 DHT22 IoT Sensor v2.1 ===");
  Serial.println("Enhanced Telemetry Edition");
  
  // Initialize performance timing
  perf.startTime = millis();
  
  // Initialize EEPROM and load persistent data
  EEPROM.begin(EEPROM_SIZE);
  loadPersistentData();
  
  // Initialize LED for status indication
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED off initially
  
  // Initialize DHT22 sensor
  dht.begin();
  Serial.println("DHT22 sensor initialized");
  
  // Connect to WiFi with timing
  unsigned long wifiStart = millis();
  connectToWiFi();
  perf.wifiConnectTime = millis() - wifiStart;
  
  // Configure time sync
  configureTime();
  
  // Configure SSL client
  wifiClient.setInsecure();
  
  printSystemInfo();
  Serial.println("Setup complete. Starting main loop...");
}

void loop() {
  perf.loopExecutionTime = millis();
  digitalWrite(LED_PIN, LOW); // LED on during operation
  
  bool success = readAndProcessData();
  
  if (success) {
    Serial.println("✓ Data processed successfully");
    blinkLED(2, 200); // Success indication
    stats.successfulTransmissions++;
  } else {
    Serial.println("✗ Failed to process data");
    blinkLED(5, 100); // Error indication
    stats.failedTransmissions++;
    addError("data_process");
  }
  
  // Calculate loop execution time
  perf.loopExecutionTime = millis() - perf.loopExecutionTime;
  
  digitalWrite(LED_PIN, HIGH); // LED off
  
  // Save all persistent data before sleep
  savePersistentData();
  
  Serial.printf("Loop completed in %lu ms. Entering deep sleep for 5 minutes...\n", 
                perf.loopExecutionTime);
  Serial.flush();
  
  // Deep sleep for energy efficiency
  ESP.deepSleep(SLEEP_INTERVAL);
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi connected!");
    Serial.printf("IP: %s | RSSI: %d dBm | Channel: %d\n", 
                  WiFi.localIP().toString().c_str(), WiFi.RSSI(), WiFi.channel());
  } else {
    Serial.println("\n✗ WiFi connection failed!");
    stats.wifiDisconnects++;
    addError("wifi_timeout");
  }
}

void configureTime() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Syncing time");
  
  unsigned long startTime = millis();
  time_t now = time(nullptr);
  
  while (now < 8 * 3600 * 2 && millis() - startTime < 10000) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  
  if (now >= 8 * 3600 * 2) {
    Serial.println("\n✓ Time synchronized");
  } else {
    Serial.println("\n⚠ Time sync timeout - using relative timestamps");
    addError("time_sync");
  }
}

bool readAndProcessData() {
  SensorData data;
  
  if (!readSensorData(data)) {
    stats.failureCount++;
    addError("sensor_read");
    return false;
  }
  
  // Calculate enhanced environmental data
  calculateEnhancedMetrics(data);
  
  // Validate readings
  if (!validateReadings(data.temperature, data.humidity)) {
    Serial.println("⚠ Invalid sensor readings detected");
    stats.failureCount++;
    addError("invalid_data");
    return false;
  }
  
  // Update 24-hour statistics
  updateDailyStats(data.temperature, data.humidity);
  
  // Check if significant change occurred
  bool significantChange = hasSignificantChange(data.temperature, data.humidity);
  bool isFullTelemetryTime = (stats.totalReadings % FULL_TELEMETRY_INTERVAL == 0);
  
  Serial.printf("🌡️ %.1f°F | 💧 %.1f%% | 🔥 %.1f°F | 💎 %.1f°F | %s\n", 
                data.temperature, data.humidity, data.heatIndex, data.dewPoint, 
                data.comfortLevel.c_str());
  
  if (significantChange) Serial.println("📈 Significant change detected");
  if (isFullTelemetryTime) Serial.println("📊 Full telemetry transmission");
  
  // Determine transmission strategy
  bool shouldSend = (stats.totalReadings == 0 || significantChange || isFullTelemetryTime);
  
  if (shouldSend) {
    if (sendDataWithRetry(data, isFullTelemetryTime)) {
      updateLastReadings(data.temperature, data.humidity);
      stats.totalReadings++;
      return true;
    } else {
      stats.apiErrors++;
      addError("api_failed");
      return false;
    }
  } else {
    // Update stats but don't send
    updateLastReadings(data.temperature, data.humidity);
    stats.totalReadings++;
    Serial.println("📡 Data cached (no transmission needed)");
    return true;
  }
}

bool readSensorData(SensorData& data) {
  Serial.println("📡 Reading DHT22 sensor...");
  unsigned long sensorStart = millis();
  
  // Multiple reading attempts for reliability
  for (int attempt = 0; attempt < 3; attempt++) {
    data.humidity = dht.readHumidity();
    data.temperature = dht.readTemperature(true); // Fahrenheit
    data.timestamp = time(nullptr);
    
    if (!isnan(data.humidity) && !isnan(data.temperature)) {
      perf.sensorReadTime = millis() - sensorStart;
      return true;
    }
    
    Serial.printf("⚠ Reading attempt %d failed, retrying...\n", attempt + 1);
    delay(2000); // DHT22 needs time between readings
  }
  
  Serial.println("✗ All sensor reading attempts failed!");
  perf.sensorReadTime = millis() - sensorStart;
  return false;
}

void calculateEnhancedMetrics(SensorData& data) {
  // Heat Index calculation (feels-like temperature)
  if (data.temperature >= 80.0 && data.humidity >= 40.0) {
    data.heatIndex = calculateHeatIndex(data.temperature, data.humidity);
  } else {
    data.heatIndex = data.temperature; // Heat index not applicable
  }
  
  // Dew Point calculation
  data.dewPoint = calculateDewPoint(data.temperature, data.humidity);
  
  // Absolute Humidity (grams of water per cubic meter of air)
  data.absoluteHumidity = calculateAbsoluteHumidity(data.temperature, data.humidity);
  
  // Comfort Level assessment
  data.comfortLevel = assessComfortLevel(data.temperature, data.humidity);
}

float calculateHeatIndex(float tempF, float humidity) {
  // Rothfusz regression for heat index
  float hi = -42.379 + 2.04901523 * tempF + 10.14333127 * humidity
           - 0.22475541 * tempF * humidity - 6.83783e-3 * tempF * tempF
           - 5.481717e-2 * humidity * humidity + 1.22874e-3 * tempF * tempF * humidity
           + 8.5282e-4 * tempF * humidity * humidity - 1.99e-6 * tempF * tempF * humidity * humidity;
  return hi;
}

float calculateDewPoint(float tempF, float humidity) {
  float tempC = (tempF - 32.0) * 5.0 / 9.0;
  float a = 17.27;
  float b = 237.7;
  float alpha = ((a * tempC) / (b + tempC)) + log(humidity / 100.0);
  float dewC = (b * alpha) / (a - alpha);
  return dewC * 9.0 / 5.0 + 32.0; // Convert back to Fahrenheit
}

float calculateAbsoluteHumidity(float tempF, float humidity) {
  float tempC = (tempF - 32.0) * 5.0 / 9.0;
  float saturationVaporPressure = 6.112 * exp((17.67 * tempC) / (tempC + 243.5));
  float actualVaporPressure = (humidity / 100.0) * saturationVaporPressure;
  float absoluteHumidity = (actualVaporPressure * 2.1674) / (tempC + 273.15);
  return absoluteHumidity;
}

String assessComfortLevel(float tempF, float humidity) {
  if (tempF < 60.0) return "cold";
  if (tempF > 85.0) return "hot";
  if (humidity > 70.0) return "humid";
  if (humidity < 30.0) return "dry";
  if (tempF >= 68.0 && tempF <= 78.0 && humidity >= 40.0 && humidity <= 60.0) {
    return "optimal";
  }
  return "comfortable";
}

void updateDailyStats(float temperature, float humidity) {
  if (dailyStats.readingCount24h == 0) {
    // First reading of the day
    dailyStats.avgTemp24h = temperature;
    dailyStats.minTemp24h = temperature;
    dailyStats.maxTemp24h = temperature;
    dailyStats.avgHumidity24h = humidity;
    dailyStats.minHumidity24h = humidity;
    dailyStats.maxHumidity24h = humidity;
    dailyStats.readingCount24h = 1;
  } else {
    // Update running averages and extremes
    dailyStats.avgTemp24h = (dailyStats.avgTemp24h * dailyStats.readingCount24h + temperature) / (dailyStats.readingCount24h + 1);
    dailyStats.avgHumidity24h = (dailyStats.avgHumidity24h * dailyStats.readingCount24h + humidity) / (dailyStats.readingCount24h + 1);
    
    if (temperature < dailyStats.minTemp24h) dailyStats.minTemp24h = temperature;
    if (temperature > dailyStats.maxTemp24h) dailyStats.maxTemp24h = temperature;
    if (humidity < dailyStats.minHumidity24h) dailyStats.minHumidity24h = humidity;
    if (humidity > dailyStats.maxHumidity24h) dailyStats.maxHumidity24h = humidity;
    
    dailyStats.readingCount24h++;
    
    // Reset daily stats after 24 hours (288 readings at 5-minute intervals)
    if (dailyStats.readingCount24h > 288) {
      dailyStats.readingCount24h = 0;
    }
  }
}

bool validateReadings(float temperature, float humidity) {
  return (temperature >= MIN_TEMP_F && temperature <= MAX_TEMP_F &&
          humidity >= MIN_HUMIDITY && humidity <= MAX_HUMIDITY);
}

bool hasSignificantChange(float currentTemp, float currentHumidity) {
  if (stats.totalReadings == 0) return true; // First reading
  
  float tempDiff = abs(currentTemp - stats.lastTemperature);
  float humidityDiff = abs(currentHumidity - stats.lastHumidity);
  
  return (tempDiff >= TEMP_CHANGE_THRESHOLD || humidityDiff >= HUMIDITY_CHANGE_THRESHOLD);
}

bool sendDataWithRetry(const SensorData& data, bool fullTelemetry) {
  for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("📶 WiFi disconnected, attempting reconnection...");
      connectToWiFi();
      if (WiFi.status() != WL_CONNECTED) {
        delay(RETRY_DELAY);
        continue;
      }
    }
    
    if (sendToAPI(data, fullTelemetry)) {
      return true;
    }
    
    Serial.printf("🔄 Send attempt %d failed, retrying in %lu ms...\n", 
                  attempt + 1, RETRY_DELAY);
    delay(RETRY_DELAY);
  }
  
  Serial.println("💥 All send attempts failed!");
  return false;
}

bool sendToAPI(const SensorData& data, bool fullTelemetry) {
  unsigned long apiStart = millis();
  Serial.printf("📤 Sending %s telemetry to API...\n", fullTelemetry ? "FULL" : "basic");
  
  http.setTimeout(API_TIMEOUT);
  http.begin(wifiClient, apiEndpoint);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "ESP8266-DHT22-v2.1");
  
  // Create JSON payload based on telemetry level
  DynamicJsonDocument jsonDoc(fullTelemetry ? 1024 : 300);
  
  // Core sensor data (always included)
  JsonObject sensorData = jsonDoc.createNestedObject("sensor_data");
  sensorData["temperature"] = round(data.temperature * 10) / 10.0;
  sensorData["temperature_unit"] = "F";
  sensorData["humidity"] = round(data.humidity * 10) / 10.0;
  sensorData["heat_index"] = round(data.heatIndex * 10) / 10.0;
  sensorData["dew_point"] = round(data.dewPoint * 10) / 10.0;
  sensorData["absolute_humidity"] = round(data.absoluteHumidity * 10) / 10.0;
  sensorData["comfort_level"] = data.comfortLevel;
  
  // Basic metadata (always included)
  JsonObject metadata = jsonDoc.createNestedObject("metadata");
  metadata["timestamp"] = data.timestamp;
  metadata["timestamp_iso"] = getISO8601Timestamp(data.timestamp);
  metadata["local_time"] = getLocalTimeString(data.timestamp);
  metadata["device_id"] = WiFi.macAddress();
  metadata["sensor_type"] = "DHT22";
  metadata["firmware_version"] = firmwareVersion;
  metadata["transmission_type"] = fullTelemetry ? "full" : "basic";
  
  if (fullTelemetry) {
    // Device information
    JsonObject deviceInfo = jsonDoc.createNestedObject("device_info");
    deviceInfo["chip_id"] = String(ESP.getChipId(), HEX);
    deviceInfo["flash_chip_id"] = String(ESP.getFlashChipId(), HEX);
    deviceInfo["sketch_size"] = ESP.getSketchSize();
    deviceInfo["free_sketch_space"] = ESP.getFreeSketchSpace();
    deviceInfo["sdk_version"] = ESP.getSdkVersion();
    deviceInfo["boot_version"] = ESP.getBootVersion();
    deviceInfo["reset_reason"] = ESP.getResetReason();
    
    // Power management
    JsonObject powerMgmt = jsonDoc.createNestedObject("power_management");
    powerMgmt["vcc_voltage"] = ESP.getVcc() / 1024.0;
    powerMgmt["deep_sleep_duration"] = SLEEP_INTERVAL / 1000000;
    powerMgmt["wake_reason"] = "timer";
    
    // Connectivity
    JsonObject connectivity = jsonDoc.createNestedObject("connectivity");
    connectivity["wifi_ssid"] = WiFi.SSID();
    connectivity["wifi_rssi"] = WiFi.RSSI();
    connectivity["wifi_quality"] = getWiFiQuality(WiFi.RSSI());
    connectivity["wifi_channel"] = WiFi.channel();
    connectivity["ip_address"] = WiFi.localIP().toString();
    connectivity["gateway"] = WiFi.gatewayIP().toString();
    
    // Performance metrics
    JsonObject performance = jsonDoc.createNestedObject("performance");
    performance["free_heap"] = ESP.getFreeHeap();
    performance["heap_fragmentation"] = ESP.getHeapFragmentation();
    performance["max_free_block"] = ESP.getMaxFreeBlockSize();
    performance["cpu_frequency"] = ESP.getCpuFreqMHz();
    performance["uptime"] = getReadableUptime();
    performance["wifi_connect_time_ms"] = perf.wifiConnectTime;
    performance["sensor_read_time_ms"] = perf.sensorReadTime;
    performance["loop_execution_time_ms"] = perf.loopExecutionTime;
    
    // Statistics
    JsonObject statistics = jsonDoc.createNestedObject("statistics");
    statistics["total_readings"] = stats.totalReadings;
    statistics["successful_transmissions"] = stats.successfulTransmissions;
    statistics["failed_transmissions"] = stats.failedTransmissions;
    statistics["sensor_failures"] = stats.failureCount;
    statistics["wifi_disconnects"] = stats.wifiDisconnects;
    statistics["api_errors"] = stats.apiErrors;
    
    if (dailyStats.readingCount24h > 0) {
      statistics["average_temperature_24h"] = round(dailyStats.avgTemp24h * 10) / 10.0;
      statistics["min_temperature_24h"] = round(dailyStats.minTemp24h * 10) / 10.0;
      statistics["max_temperature_24h"] = round(dailyStats.maxTemp24h * 10) / 10.0;
      statistics["average_humidity_24h"] = round(dailyStats.avgHumidity24h * 10) / 10.0;
      statistics["min_humidity_24h"] = round(dailyStats.minHumidity24h * 10) / 10.0;
      statistics["max_humidity_24h"] = round(dailyStats.maxHumidity24h * 10) / 10.0;
    }
    
    float successRate = stats.totalReadings > 0 ? 
                       (float(stats.successfulTransmissions) / float(stats.totalReadings)) * 100.0 : 100.0;
    statistics["data_quality_score"] = round(successRate * 10) / 10.0;
    
    // Error diagnostics
    JsonObject diagnostics = jsonDoc.createNestedObject("diagnostics");
    JsonArray errorArray = diagnostics.createNestedArray("recent_errors");
    for (int i = 0; i < 5; i++) {
      if (strlen(errorHistory.lastErrors[i]) > 0) {
        errorArray.add(errorHistory.lastErrors[i]);
      }
    }
  }
  
  String jsonString;
  serializeJson(jsonDoc, jsonString);
  
  Serial.printf("📦 Payload size: %d bytes\n", jsonString.length());
  
  int httpResponseCode = http.POST(jsonString);
  perf.apiResponseTime = millis() - apiStart;
  
  bool success = false;
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.printf("📡 HTTP %d: %s (response time: %lu ms)\n", 
                  httpResponseCode, response.c_str(), perf.apiResponseTime);
    success = (httpResponseCode >= 200 && httpResponseCode < 300);
  } else {
    Serial.printf("💥 HTTP Error: %d (%s)\n", httpResponseCode, getHttpErrorString(httpResponseCode).c_str());
  }
  
  http.end();
  return success;
}

String getWiFiQuality(int rssi) {
  if (rssi > -50) return "excellent";
  if (rssi > -60) return "good";
  if (rssi > -70) return "fair";
  return "poor";
}

String getHttpErrorString(int errorCode) {
  switch(errorCode) {
    case -1: return "Connection refused";
    case -2: return "Send header failed";
    case -3: return "Send payload failed";
    case -4: return "Not connected";
    case -5: return "Connection lost";
    case -6: return "No stream";
    case -7: return "No HTTP server";
    case -8: return "Too less RAM";
    case -9: return "Encoding error";
    case -10: return "Stream write error";
    case -11: return "Read timeout";
    default: return "Unknown error";
  }
}

void updateLastReadings(float temperature, float humidity) {
  stats.lastTemperature = temperature;
  stats.lastHumidity = humidity;
}

void addError(const char* errorType) {
  strncpy(errorHistory.lastErrors[errorHistory.errorIndex], errorType, 15);
  errorHistory.lastErrors[errorHistory.errorIndex][15] = '\0';
  errorHistory.errorIndex = (errorHistory.errorIndex + 1) % 5;
}

void loadPersistentData() {
  // Load basic stats
  EEPROM.get(ADDR_LAST_TEMP, stats.lastTemperature);
  EEPROM.get(ADDR_LAST_HUMIDITY, stats.lastHumidity);
  EEPROM.get(ADDR_FAILURE_COUNT, stats.failureCount);
  EEPROM.get(ADDR_TOTAL_READINGS, stats.totalReadings);
  
  // Load 24-hour stats
  EEPROM.get(ADDR_24H_STATS, dailyStats);
  
  // Load error history
  EEPROM.get(ADDR_ERROR_HISTORY, errorHistory);
  
  // Initialize if first run (check for NaN values)
  if (isnan(stats.lastTemperature)) {
    stats.lastTemperature = 70.0;
    stats.lastHumidity = 50.0;
    stats.failureCount = 0;
    stats.totalReadings = 0;
    stats.successfulTransmissions = 0;
    stats.failedTransmissions = 0;
    stats.wifiDisconnects = 0;
    stats.apiErrors = 0;
    
    memset(&dailyStats, 0, sizeof(dailyStats));
    memset(&errorHistory, 0, sizeof(errorHistory));
  }
  
  Serial.printf("📊 Loaded stats - Readings: %u, Failures: %u, Success Rate: %.1f%%\n", 
                stats.totalReadings, stats.failureCount,
                stats.totalReadings > 0 ? (float(stats.successfulTransmissions) / float(stats.totalReadings)) * 100.0 : 100.0);
}

void savePersistentData() {
  EEPROM.put(ADDR_LAST_TEMP, stats.lastTemperature);
  EEPROM.put(ADDR_LAST_HUMIDITY, stats.lastHumidity);
  EEPROM.put(ADDR_FAILURE_COUNT, stats.failureCount);
  EEPROM.put(ADDR_TOTAL_READINGS, stats.totalReadings);
  EEPROM.put(ADDR_24H_STATS, dailyStats);
  EEPROM.put(ADDR_ERROR_HISTORY, errorHistory);
  EEPROM.commit();
}

void printSystemInfo() {
  Serial.println("\n=== SYSTEM INFORMATION ===");
  Serial.printf("Chip ID: %08X\n", ESP.getChipId());
  Serial.printf("Flash ID: %08X\n", ESP.getFlashChipId());
  Serial.printf("CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash Size: %d bytes\n", ESP.getFlashChipSize());
  Serial.printf("Sketch Size: %d bytes\n", ESP.getSketchSize());
  Serial.printf("Free Heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("SDK Version: %s\n", ESP.getSdkVersion());
  Serial.printf("Boot Version: %d\n", ESP.getBootVersion());
  Serial.printf("VCC: %.2f V\n", ESP.getVcc() / 1024.0);
  Serial.println("==========================\n");
}

String getISO8601Timestamp(time_t timestamp) {
  struct tm* timeinfo = gmtime(&timestamp);
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", timeinfo);
  return String(buffer);
}

String getLocalTimeString(time_t timestamp) {
  // Convert to Central Time (UTC-6 for standard time, UTC-5 for daylight)
  // For simplicity, using UTC-5 (CDT)
  time_t localTime = timestamp - (5 * 3600);
  struct tm* timeinfo = gmtime(&localTime);
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S CDT", timeinfo);
  return String(buffer);
}

String getReadableUptime() {
  unsigned long uptime = millis() / 1000;
  unsigned long days = uptime / 86400;
  uptime %= 86400;
  unsigned long hours = uptime / 3600;
  uptime %= 3600;
  unsigned long minutes = uptime / 60;
  unsigned long seconds = uptime % 60;
  
  char buffer[50];
  if (days > 0) {
    sprintf(buffer, "%lud %luh %lum %lus", days, hours, minutes, seconds);
  } else if (hours > 0) {
    sprintf(buffer, "%luh %lum %lus", hours, minutes, seconds);
  } else if (minutes > 0) {
    sprintf(buffer, "%lum %lus", minutes, seconds);
  } else {
    sprintf(buffer, "%lus", seconds);
  }
  return String(buffer);
}

void blinkLED(int count, int delayMs) {
  for (int i = 0; i < count; i++) {
    digitalWrite(LED_PIN, LOW);  // LED on
    delay(delayMs);
    digitalWrite(LED_PIN, HIGH); // LED off
    delay(delayMs);
  }
}