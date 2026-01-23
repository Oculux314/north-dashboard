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

const int LED_PIN = 25;
const int VOLTAGE_PIN = 32;
const int CURRENT_PIN = 33;

ESP32Time rtc(0);

// MARK: STATE

enum WifiState { OFFLINE, ONLINE_LED, ONLINE, TRANSMITTING };

struct Reading {
  float voltage = 0.0;
  float current = 0.0;
};

struct BatchReading {
  float voltage = 0.0;
  float current = 0.0;
  unsigned long long timestamp = 0; // millis past epoch (tempory values before RTC sync)
};

struct State {
  WifiState wifiState = OFFLINE;
  unsigned long long wifiConnectedEndMillis = 0;
  unsigned long long nextBatchMillis = 0;
  // Sync RTC to millis
  unsigned long millisSynced = 0;
  unsigned long rtcSynced = 0;
};

volatile State state;
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

WiFiClientSecure transmitBatch();
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

  // Time
  configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // Periodically syncs RTC
  state.nextBatchMillis = getMillisPastEpoch() + BATCH_MILLIS;
}

// MARK: LOOP

void loop() {
  updateLed();
  updateWifi();
  updateReadings();
}

// MARK: SWITCH STATE

void switchState(WifiState newState) {
  switch (newState) {
    case OFFLINE:
      initWifi();
      break;
    case ONLINE_LED:
      state.wifiConnectedEndMillis = getMillisPastEpoch() + 500;
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
      digitalWrite(LED_PIN, getMillisPastEpoch() % 2000 < 200 ? HIGH : LOW);
      break;
    case ONLINE_LED:
      digitalWrite(LED_PIN, getMillisPastEpoch() % 100 < 25 ? HIGH : LOW);
      if (getMillisPastEpoch() > state.wifiConnectedEndMillis) {
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
  if (!batchReadingsBuffer.empty() && state.wifiState == ONLINE) {
    transmitBatch();
  }

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

    Serial.printf("CurrentMillis: %llu\n", currentMillis);
    Serial.printf("BATCH_MILLIS:  %d\n", BATCH_MILLIS);
    Serial.printf("Next batch at: %llu\n", state.nextBatchMillis);
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

WiFiClientSecure transmitBatch() {
  if (batchReadingsBuffer.empty() || !rtcSynced()) {
    return;
  }
  if (state.rtcSynced == 0) {
    // First time RTC detected as synced
    onRtcSync();
  }
  Serial.printf("Transmitting at time %llu\n", getMillisPastEpoch());

  BatchReading reading = batchReadingsBuffer.front();
  batchReadingsBuffer.pop(); // Ignore failures lol

  WiFiClientSecure client;
  client.setInsecure();  // Accept any certificate
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

  // Serial.printf(
  //     "POST /api/mutation HTTP/1.1\r\n"
  //     "Host: grateful-ibis-516.convex.cloud\r\n"
  //     "Content-Type: application/json\r\n"
  //     "Content-Length: %d\r\n"
  //     "\r\n"
  //     "%s\r\n",
  //     bodyLen, body);
  client.printf(
      "POST /api/mutation HTTP/1.1\r\n"
      "Host: grateful-ibis-516.convex.cloud\r\n"
      "Content-Type: application/json\r\n"
      "Content-Length: %d\r\n"
      "\r\n"
      "%s\r\n",
      bodyLen, body);

  // unsigned long long start = getMillisPastEpoch();
  // while (client.connected() && (getMillisPastEpoch() - start < 5000)) {  // 5s timeout
  //   while (client.available()) {
  //     Serial.println(client.readStringUntil('\n'));
  //   }
  // }
  // client.stop();

  return client;
}

// MARK: RTC

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
}
