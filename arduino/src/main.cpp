#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <ESP32Time.h>
#include <queue>
#include <vector>

// MARK: CONSTANTS

const String WIFI_SSID = "WDG";
const String WIFI_PASSWORD = "strawberry";
const char* BACKEND_SERVER = "grateful-ibis-516.convex.cloud";

const int BATCH_MILLIS = 10000;
const int REQUEST_TIMEOUT_MILLIS = 5000;
const int REQUEST_MAX_RETRIES = 3;
const int MAX_RESPONSE_SIZE = 512;

const int LED_PIN = 25;
const int VOLTAGE_PIN = 32;
const int CURRENT_PIN = 33;

ESP32Time rtc(0);
WiFiClientSecure client;

// MARK: STATE

enum WifiState { OFFLINE, PENDING_TIME_SYNC, ONLINE, TRANSMITTING };

struct Reading {
  float voltage = 0.0;
  float current = 0.0;
};

struct BatchReading {
  float voltage = 0.0;
  float current = 0.0;
  unsigned long long timestamp = 0; // millis past epoch (tempory values before RTC sync)
};

struct Request {
  BatchReading reading;
  unsigned long long lastSentMillis = 0;
  int retries = 0;
  char response[MAX_RESPONSE_SIZE];
  int responseLen = 0;
};

struct State {
  volatile WifiState wifiState = OFFLINE;
  volatile unsigned long long rtcSyncFlashEndMillis = 0;
  unsigned long long nextBatchMillis = 0;
  // Sync RTC to millis
  unsigned long millisSynced = 0;
  unsigned long rtcSynced = 0;
  Request currentRequest;
};

State state;

// I don't wanna deal with memory allocations xD
std::queue<BatchReading> batchReadingsBuffer;
Reading readingSum;
int readingCount = 0;

// MARK: DECLARATIONS

void switchState(WifiState newState);

void updateWifi();
void onWifiConnect(WiFiEvent_t event, WiFiEventInfo_t info);
void onWifiDisconnect(WiFiEvent_t event, WiFiEventInfo_t info);
void initWifi();

void updateLed();

void updateReadings();
float readVoltage();
float readCurrent();

void transmitBatch(BatchReading reading);
void readTransmitResponse();
bool requestSucceeded(char* response);
void retryTransmittion();

void updateRtc();
bool rtcSynced();
unsigned long long getMillisPastEpoch();
void onRtcSync();

// MARK: SETUP

void setup() {
  Serial.begin(115200);

  // Pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(VOLTAGE_PIN, INPUT);
  pinMode(CURRENT_PIN, INPUT);
  digitalWrite(LED_PIN, HIGH);  // In case smth really breaks

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.onEvent(onWifiConnect, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(onWifiDisconnect,
               WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  initWifi();
  client.setInsecure();  // Accept any SSL certificate

  // Time
  configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // Periodically syncs RTC
  state.nextBatchMillis = getMillisPastEpoch() + BATCH_MILLIS;
}

// MARK: LOOP

void loop() {
  updateLed();
  updateRtc();
  updateWifi();
  updateReadings();
}

// MARK: SWITCH STATE

void switchState(WifiState newState) {
  Serial.printf("Switching WiFi state from %d to %d\n", state.wifiState,
                newState);
  switch (newState) {
    case OFFLINE:
      initWifi();
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
  switchState(PENDING_TIME_SYNC);
}

void onWifiDisconnect(WiFiEvent_t event, WiFiEventInfo_t info) {
  switchState(OFFLINE);
}

void initWifi() { WiFi.begin(WIFI_SSID, WIFI_PASSWORD); }

// MARK: LED

void updateLed() {
  Serial.printf("LED State Update: WiFi State %d (%llu)\n", state.wifiState, getMillisPastEpoch());
  if (getMillisPastEpoch() < state.rtcSyncFlashEndMillis) {
    Serial.println("RTC Sync Flash");
    // Flash 5 times to indicate RTC sync ready state
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, getMillisPastEpoch() % 100 < 25 ? HIGH : LOW);
    return;
  }

  switch (state.wifiState) {
    case OFFLINE:
      pinMode(LED_PIN, OUTPUT);
      digitalWrite(LED_PIN, getMillisPastEpoch() % 2000 < 200 ? HIGH : LOW);
      break;
    case PENDING_TIME_SYNC:
      pinMode(LED_PIN, OUTPUT);
      digitalWrite(LED_PIN, getMillisPastEpoch() % 1000 < 200 ? HIGH : LOW);
      break;
    case ONLINE:
      pinMode(LED_PIN, OUTPUT);
      digitalWrite(LED_PIN, LOW);
      break;
    case TRANSMITTING:
      analogWrite(LED_PIN, 1);
      break;
    default:
      // Should not happen
      pinMode(LED_PIN, OUTPUT);
      digitalWrite(LED_PIN, HIGH);
      break;
  }
}

// MARK: READINGS

void updateReadings() {
  // Transmit (if needed)
  if (!batchReadingsBuffer.empty() && state.wifiState == ONLINE &&
      rtcSynced()) {
    Serial.println("Attempting transmission of stored batch...");
    BatchReading reading = batchReadingsBuffer.front();
    batchReadingsBuffer.pop();  // Ignore failures lol
    transmitBatch(reading);
  }

  // Read transmit response (if needed)
  if (state.wifiState == TRANSMITTING) {
    readTransmitResponse();
  }

  // Create batch (if needed)
  unsigned long long currentMillis = getMillisPastEpoch();
  if (currentMillis > state.nextBatchMillis) {
    // Store batch
    BatchReading avgReading;
    avgReading.voltage = readingSum.voltage / readingCount;
    avgReading.current = readingSum.current / readingCount;
    avgReading.timestamp = currentMillis -
                           (BATCH_MILLIS / 2);  // Approximate middle of batch
    batchReadingsBuffer.push(avgReading);
    Serial.printf("Batch stored: V=%f, I=%f\n", avgReading.voltage,
                  avgReading.current);
    // Reset
    readingSum = Reading();
    readingCount = 0;
    state.nextBatchMillis += BATCH_MILLIS;
    if (state.nextBatchMillis < currentMillis) {
      // Handle time sync
      state.nextBatchMillis = currentMillis + BATCH_MILLIS;
    }
  }

  // Perform read
  readingSum.voltage += readVoltage();
  readingSum.current += readCurrent();
  readingCount++;
}

// VREF=3.3V, ADC_MAX=4096 R1=1.0M, R2=0.2M
float VOLTAGE_RATIO = 3.3 / 4096.0 * (10 + 2) / 2.0;
float readVoltage() {
  int reading = analogRead(VOLTAGE_PIN);
  float voltage = ((reading + 0.5) * VOLTAGE_RATIO);
  return voltage;
}

// VREF=3.3V, ADC_MAX=4096 R1=1M, R2=1M, 5V = 300A
float CURRENT_RATIO = 3.3 / 4096.0 * (1 + 1) / 1.0 * 300 / 5.0;
float readCurrent() {
  int reading = analogRead(CURRENT_PIN);
  float current = ((reading + 0.5) * CURRENT_RATIO);
  return current;
}

// MARK: WIFI TRANSMISSION

void transmitBatch(BatchReading reading) {
  Serial.printf("Transmitting batch: V=%f, I=%f, T=%llu\n", reading.voltage,
                reading.current, reading.timestamp);

  // Setup request state
  state.currentRequest.reading.current = reading.current;
  state.currentRequest.reading.voltage = reading.voltage;
  state.currentRequest.reading.timestamp = reading.timestamp;
  state.currentRequest.lastSentMillis = getMillisPastEpoch();
  state.currentRequest.retries = 0;
  state.currentRequest.responseLen = 0;
  switchState(TRANSMITTING);

  if (!client.connect(BACKEND_SERVER, 443)) {
    Serial.println("Not connected to backend server");
    return;
  }

  // Needs to be fast in a loop (128 is overkill - body is usually 104 chars)
  char body[128];
  int bodyLen =
      snprintf(body, sizeof(body),
               "{\"path\":\"api:postLog\",\"args\":{\"timestamp\":%llu,"
               "\"voltage\":%.6f,\"current\":%.6f},\"format\":\"json\"}",
               reading.timestamp, reading.voltage, reading.current);

  client.printf(
      "POST /api/mutation HTTP/1.1\r\n"
      "Host: grateful-ibis-516.convex.cloud\r\n"
      "Content-Type: application/json\r\n"
      "Content-Length: %d\r\n"
      "\r\n"
      "%s\r\n",
      bodyLen, body);
}

void readTransmitResponse() {
  // Check status
  bool isTimeout = getMillisPastEpoch() - state.currentRequest.lastSentMillis >
                   REQUEST_TIMEOUT_MILLIS;
  bool exceededResponseSize = state.currentRequest.responseLen >= MAX_RESPONSE_SIZE - 1;
  if (exceededResponseSize) {
    // Prevent overflow for null terminator
    state.currentRequest.responseLen = MAX_RESPONSE_SIZE - 1;
  }

  if (!client.connected() || isTimeout || exceededResponseSize) {
    Serial.println("Something happened...");
    client.stop();
    state.currentRequest.response[state.currentRequest.responseLen] = '\0'; // Ensure null-terminate exists
    if (requestSucceeded(state.currentRequest.response)) {
      Serial.println("Request succeeded.");
      // Request succeeded
      switchState(ONLINE);
    } else if (state.currentRequest.retries < REQUEST_MAX_RETRIES) {
      // Failed - retry
      Serial.printf("Request failed, retrying... (attempt %d)\n",
                    state.currentRequest.retries + 1);
      retryTransmittion();
    } else {
      // Failed - give up
      Serial.println("Request failed, giving up.");
      switchState(ONLINE);
    }
  }

  // Read
  if (client.available()) {
    state.currentRequest.response[state.currentRequest.responseLen++] =
        client.read();
  }
}

bool requestSucceeded(char* response) {
  // Check for `"status":"success"` in response
  char *strstrResult = strstr(response, "\"status\":\"success\"");
  Serial.printf("strstr result: %s\n", strstrResult);
  return strstrResult != nullptr;
}

void retryTransmittion() {
  state.currentRequest.retries++;
  state.currentRequest.lastSentMillis = getMillisPastEpoch();
  state.currentRequest.responseLen = 0;
  transmitBatch(state.currentRequest.reading);
}

// MARK: RTC

void updateRtc() {
  switch (state.wifiState) {
    case PENDING_TIME_SYNC:
      if (rtcSynced()) {
        if (state.rtcSynced == 0) {
          onRtcSync();
        }
        switchState(ONLINE);
      }
      break;
    default:
      break;
  }
  // Run onRtcSync once when RTC is synced
}

bool rtcSynced() {
  time_t now = rtc.getEpoch();
  return now > 1609459200;  // Jan 1, 2021
}

// Returns milliseconds past epoch, handling millis() overflow
// Returns regular millis() before RTC sync
unsigned long long getMillisPastEpoch() {
  unsigned long rawMillis = millis();
  unsigned long millisSinceSync;
  if (rawMillis < state.millisSynced) {
    // millis() overflowed
    millisSinceSync = rawMillis + (ULONG_MAX - state.millisSynced); // Brackets to avoid overflow again
  } else {
    millisSinceSync = rawMillis - state.millisSynced;
  }

  return ((unsigned long long)state.rtcSynced) * 1000 + millisSinceSync;
}

// Flags sync time and retroactively updates reading timestamps
void onRtcSync() {
  // Record sync time
  state.rtcSynced = rtc.getEpoch();
  state.millisSynced = millis();

  // Retroactively update timestamps
  std::vector<BatchReading> tempBuffer;
  while (!batchReadingsBuffer.empty()) {
    BatchReading reading = batchReadingsBuffer.front();
    batchReadingsBuffer.pop();
    // Calculate new timestamp
    reading.timestamp = state.rtcSynced + (reading.timestamp - state.millisSynced) / 1000;
    tempBuffer.push_back(reading);
  }
  for (BatchReading& reading : tempBuffer) {
    batchReadingsBuffer.push(reading);
  }

  // Flash LED to indicate RTC sync
  state.rtcSyncFlashEndMillis = getMillisPastEpoch() + 500;
  Serial.printf("Flash ends at millis %llu\n", state.rtcSyncFlashEndMillis);
}
