#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include <queue>

// MARK: CONSTANTS

const String WIFI_SSID = "WDG";
const String WIFI_PASSWORD = "strawberry";

const int BATCH_MILLIS = 1000;

const int LED_PIN = 25;
const int VOLTAGE_PIN = 32;
const int CURRENT_PIN = 33;

// MARK: STATE

enum LedState { WIFI_CONNECTING, WIFI_CONNECTED, IDLE };

struct Reading {
  float voltage;
  float current;
};

struct State {
  LedState ledState = WIFI_CONNECTING;
  int wifiConnectedEndMillis = 0;
  int nextBatchMillis = 0;
};

volatile State state;
// I don't wanna deal with memory allocations xD
std::queue<Reading> batchReadingsBuffer;
std::vector<Reading> readingsLog;

// MARK: DECLARATIONS

void onWifiConnect(WiFiEvent_t event, WiFiEventInfo_t info);
void onWifiDisconnect(WiFiEvent_t event, WiFiEventInfo_t info);
void initWiFi();

void updateLed();
void switchLedState(LedState newState);

void updateReadings();
Reading createReading();
float readVoltage();
float readCurrent();
float avg(const std::vector<float>& readings);

// MARK: SETUP

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(VOLTAGE_PIN, INPUT);
  pinMode(CURRENT_PIN, INPUT);

  digitalWrite(LED_PIN, HIGH);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  initWiFi();
  WiFi.onEvent(onWifiConnect, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(onWifiDisconnect,
               WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
}

// MARK: LOOP

void loop() {
  updateLed();
  updateReadings();
}

// MARK: WIFI

void onWifiConnect(WiFiEvent_t event, WiFiEventInfo_t info) {
  switchLedState(WIFI_CONNECTED);
}

void onWifiDisconnect(WiFiEvent_t event, WiFiEventInfo_t info) {
  initWiFi();
  switchLedState(WIFI_CONNECTING);
}

void initWiFi() { WiFi.begin(WIFI_SSID, WIFI_PASSWORD); }

// MARK: LED

void updateLed() {
  switch (state.ledState) {
    case WIFI_CONNECTING:
      digitalWrite(LED_PIN, millis() % 2000 < 200 ? HIGH : LOW);
      break;
    case WIFI_CONNECTED:
      digitalWrite(LED_PIN, millis() % 100 < 25 ? HIGH : LOW);
      if (millis() > state.wifiConnectedEndMillis) {
        switchLedState(IDLE);
      }
      break;
    case IDLE:
      digitalWrite(LED_PIN, LOW);
      break;
  }
}

void switchLedState(LedState newState) {
  switch (newState) {
    case WIFI_CONNECTING:
      break;
    case WIFI_CONNECTED:
      state.wifiConnectedEndMillis = millis() + 500;
      break;
    case IDLE:
      break;
  }
  state.ledState = newState;
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
    return { voltage: 0.0, current: 0.0};
  }
  float voltageSum = 0.0;
  float currentSum = 0.0;
  for (const Reading& reading : readings) {
    voltageSum += reading.voltage;
    currentSum += reading.current;
  }
  Serial.printf("Sum: %f, Count: %zu\n", voltageSum, readings.size());
  return { voltage: voltageSum / readings.size(), current: currentSum / readings.size() };
}
