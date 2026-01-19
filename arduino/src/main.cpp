#include <Arduino.h>
#include <WiFi.h>

#include <queue>
#include <vector>

// MARK: CONSTANTS

const String WIFI_SSID = "WDG";
const String WIFI_PASSWORD = "strawberry";
const char* BACKEND_SERVER = "www.google.com";

const int BATCH_MILLIS = 1000;

const int LED_PIN = 25;
const int VOLTAGE_PIN = 32;
const int CURRENT_PIN = 33;

// MARK: STATE

enum WifiState { OFFLINE, ONLINE_LED, ONLINE, TRANSMITTING };

struct Reading {
  float voltage;
  float current;
};

struct State {
  WifiState wifiState = OFFLINE;
  int wifiConnectedEndMillis = 0;
  int nextBatchMillis = 0;
};

volatile State state;
// I don't wanna deal with memory allocations xD
std::queue<Reading> batchReadingsBuffer;
std::vector<Reading> readingsLog;

// MARK: DECLARATIONS

void switchState(WifiState newState);

void updateWifi();
void onWifiConnect(WiFiEvent_t event, WiFiEventInfo_t info);
void onWifiDisconnect(WiFiEvent_t event, WiFiEventInfo_t info);
void initWifi();
void transmitBatch();

void updateLed();

void updateReadings();
Reading createReading();
float readVoltage();
float readCurrent();
Reading avg(const std::vector<Reading>& readings);

// MARK: SETUP

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(VOLTAGE_PIN, INPUT);
  pinMode(CURRENT_PIN, INPUT);
  digitalWrite(LED_PIN, HIGH);  // In case smth really breaks

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.onEvent(onWifiConnect, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(onWifiDisconnect,
               WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  initWifi();
  state.nextBatchMillis = millis() + BATCH_MILLIS;
}

// MARK: LOOP

void loop() {
  updateLed();
  updateWifi();
  updateReadings();
}

// MARK: SWITCH STATE

void switchState(WifiState newState) {
  // Serial.printf("State: %d\n", newState);
  switch (newState) {
    case OFFLINE:
      initWifi();
      break;
    case ONLINE_LED:
      state.wifiConnectedEndMillis = millis() + 500;
      break;
    default:
      break;
  }
  state.wifiState = newState;
}

// MARK: INIT WIFI

void updateWifi() {
  switch (state.wifiState) {
    default:
      break;
  }
}

void onWifiConnect(WiFiEvent_t event, WiFiEventInfo_t info) {
  switchState(ONLINE_LED);
}

void onWifiDisconnect(WiFiEvent_t event, WiFiEventInfo_t info) {
  switchState(OFFLINE);
}

void initWifi() { WiFi.begin(WIFI_SSID, WIFI_PASSWORD); }

// MARK: LED

void updateLed() {
  switch (state.wifiState) {
    case OFFLINE:
      digitalWrite(LED_PIN, millis() % 2000 < 200 ? HIGH : LOW);
      break;
    case ONLINE_LED:
      digitalWrite(LED_PIN, millis() % 100 < 25 ? HIGH : LOW);
      if (millis() > state.wifiConnectedEndMillis) {
        switchState(ONLINE);
      }
      break;
    default:
      digitalWrite(LED_PIN, LOW);
      break;
  }
}

// MARK: READINGS

void updateReadings() {
  if (millis() > state.nextBatchMillis) {
    Reading avgReading = avg(readingsLog);
    batchReadingsBuffer.push(avgReading);
    readingsLog.clear();
    state.nextBatchMillis += BATCH_MILLIS;
  }

  readingsLog.push_back(createReading());
}

Reading createReading() {
  Reading reading;
  reading.voltage = readVoltage();
  reading.current = readCurrent();
  return reading;
}

float readVoltage() {
  float volts = float(analogRead(VOLTAGE_PIN));
  // Serial.printf("Volts: %f\n", volts);
  return volts;
}

float readCurrent() { return analogRead(CURRENT_PIN); }

Reading avg(const std::vector<Reading>& readings) {
  if (readings.empty()) {
    return {voltage : 0.0, current : 0.0};
  }
  float voltageSum = 0.0;
  float currentSum = 0.0;
  for (const Reading& reading : readings) {
    voltageSum += reading.voltage;
    currentSum += reading.current;
  }
  // Serial.printf("Sum: %f, Count: %zu\n", voltageSum, readings.size());
  return {
    voltage : voltageSum / readings.size(),
    current : currentSum / readings.size()
  };
}

// MARK: WIFI TRANSMISSION

void transmitBatch() {
  // if (batchReadingsBuffer.empty()) {
  //   return;
  // }
  // Reading reading = batchReadingsBuffer.front();
  // batchReadingsBuffer.pop();

  // WiFiClient client;
  // client.connect(BACKEND_SERVER, 80);

  // if (client.connected()) {
  //   // Make a HTTP request:
  //   client.printf("GET /update?voltage=%f&current=%f HTTP/1.0\r\n",
  //                 reading.voltage, reading.current);
  //   client.println();
  // } else {
  //   Serial.println("Not connected to backend server");
  // }
}

// if (client.connect(BACKEND_SERVER, 80)) {
//   Serial.println("connected");
//   // Make a HTTP request:
//   client.println("GET /search?q=arduino HTTP/1.0");
//   client.println();
// } else {
//   Serial.println("connection failed");
// }

// if (client.available()) {
//   char c = client.read();
//   Serial.print(c);
// }