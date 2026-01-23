#include <Arduino.h>
#include <WiFiClientSecure.h>

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

// MARK: STATE

enum WifiState { OFFLINE, ONLINE_LED, ONLINE, TRANSMITTING };

struct Reading {
  float voltage = 0.0;
  float current = 0.0;
};

struct BatchReading {
  float voltage = 0.0;
  float current = 0.0;
  int timestamp = 0;
};

struct State {
  WifiState wifiState = OFFLINE;
  unsigned long wifiConnectedEndMillis = 0;
  unsigned long nextBatchMillis = 0;
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
void transmitBatch();

void updateLed();

void updateReadings();
float readVoltage();
float readCurrent();

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
  if (!batchReadingsBuffer.empty() && state.wifiState == ONLINE) {
    transmitBatch();
  }

  if (millis() > state.nextBatchMillis) {
    // Store batch
    BatchReading avgReading;
    avgReading.voltage = readingSum.voltage / readingCount;
    avgReading.current = readingSum.current / readingCount;
    avgReading.timestamp = state.nextBatchMillis -
                           (BATCH_MILLIS / 2);  // Approximate middle of batch
    batchReadingsBuffer.push(avgReading);
    Serial.printf("Batch stored: V=%f, I=%f\n", avgReading.voltage,
                  avgReading.current);
    // Reset
    readingSum = Reading();
    readingCount = 0;
    state.nextBatchMillis += BATCH_MILLIS;
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

void transmitBatch() {
  if (batchReadingsBuffer.empty()) {
    return;
  }
  BatchReading reading = batchReadingsBuffer.front();
  batchReadingsBuffer.pop();

  WiFiClientSecure client;
  client.setInsecure();  // Accept any certificate
  if (!client.connect(BACKEND_SERVER, 443)) {
    Serial.println("Not connected to backend server");
    return;
  }

  // Needs to be fast in a loop (128 is overkill - body is usually 104 chars
  // long)
  char body[128];
  int bodyLen =
      snprintf(body, sizeof(body),
               "{\"path\":\"api:postLog\",\"args\":{\"timestamp\":%u,"
               "\"voltage\":%.6f,\"current\":%.6f},\"format\":\"json\"}",
               reading.timestamp, reading.voltage, reading.current);

  Serial.printf(
      "POST /api/mutation HTTP/1.1\r\n"
      "Host: grateful-ibis-516.convex.cloud\r\n"
      "Content-Type: application/json\r\n"
      "Content-Length: %d\r\n"
      "\r\n"
      "%s\r\n",
      bodyLen, body);
  client.printf(
      "POST /api/mutation HTTP/1.1\r\n"
      "Host: grateful-ibis-516.convex.cloud\r\n"
      "Content-Type: application/json\r\n"
      "Content-Length: %d\r\n"
      "\r\n"
      "%s\r\n",
      bodyLen, body);

  // unsigned long start = millis();
  // while (client.connected() && (millis() - start < 5000)) {  // 5s timeout
  //   while (client.available()) {
  //     Serial.println(client.readStringUntil('\n'));
  //   }
  // }
  // client.stop();
}
