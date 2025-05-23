#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

#define DHT_PIN 2
#define DHT_TYPE DHT22

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* apiEndpoint = "https://your-api-domain.com/api/sensor-data";

DHT dht(DHT_PIN, DHT_TYPE);
WiFiClient wifiClient;
HTTPClient http;

unsigned long lastReading = 0;
const unsigned long readingInterval = 5 * 60 * 1000; // 5 minutes in milliseconds

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  dht.begin();
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastReading >= readingInterval || lastReading == 0) {
    readAndSendData();
    lastReading = currentTime;
  }
  
  delay(1000);
}

void readAndSendData() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print("°C, Humidity: ");
  Serial.print(humidity);
  Serial.println("%");
  
  sendToAPI(temperature, humidity);
}

void sendToAPI(float temperature, float humidity) {
  if (WiFi.status() == WL_CONNECTED) {
    http.begin(wifiClient, apiEndpoint);
    http.addHeader("Content-Type", "application/json");
    
    StaticJsonDocument<200> jsonDoc;
    jsonDoc["temperature"] = temperature;
    jsonDoc["humidity"] = humidity;
    jsonDoc["timestamp"] = WiFi.getTime();
    jsonDoc["device_id"] = WiFi.macAddress();
    
    String jsonString;
    serializeJson(jsonDoc, jsonString);
    
    int httpResponseCode = http.POST(jsonString);
    
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.print("HTTP Response: ");
      Serial.println(httpResponseCode);
      Serial.print("Response: ");
      Serial.println(response);
    } else {
      Serial.print("Error on HTTP request: ");
      Serial.println(httpResponseCode);
    }
    
    http.end();
  } else {
    Serial.println("WiFi not connected!");
  }
}