#include <Arduino.h>
#include <WiFi.h>
#include <vector>

void onWifiDisconnect(WiFiEvent_t event, WiFiEventInfo_t info);
void initWiFi();
void flashLedBlocking(int count);
void blinkLed(int highMillis);
void updateLed();
float readVoltage();
float readCurrent();
float avg(const std::vector<float> &readings);

// MARK: CONSTANTS

const String WIFI_SSID = "WDG";
const String WIFI_PASSWORD = "strawberry";

const int BATCH_MILLIS = 1000;
const int LED_HIGH_MILLIS = 100;

const int LED_PIN = 25;
const int VOLTAGE_PIN = 32;
const int CURRENT_PIN = 33;

// MARK: STATE

struct State
{
  bool wifiConnected = false;
  int nextBatchMillis = 0;
  int nextLedLowMillis = 0;
};

State state;

// MARK: SETUP

void setup()
{
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(VOLTAGE_PIN, INPUT);
  pinMode(CURRENT_PIN, INPUT);

  digitalWrite(LED_PIN, HIGH);
  initWiFi();
  WiFi.onEvent(onWifiDisconnect,           WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  state.nextBatchMillis = millis();
}

// MARK: LOOP

std::vector<float> voltageReadings;
std::vector<float> currentReadings;

void loop()
{
  voltageReadings.clear();
  currentReadings.clear();
  state.nextBatchMillis += BATCH_MILLIS;

  while (millis() < state.nextBatchMillis)
  {
    updateLed();
    voltageReadings.push_back(readVoltage());
    currentReadings.push_back(readCurrent());
  }

  float avgVoltage = avg(voltageReadings);
  float avgCurrent = avg(currentReadings);
  Serial.printf("Avg Voltage: %f\n", avgVoltage);
  Serial.printf("Avg Current: %f\n", avgCurrent);

  // blinkLed(LED_HIGH_MILLIS);
}

// MARK: ON D/C

void onWifiDisconnect(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.wifi_sta_disconnected.reason);
  Serial.println("Trying to Reconnect");
  initWiFi();
}

// MARK: HELPERS

void initWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print('.');
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
    delay(500);
  }
  Serial.println(" " + WiFi.localIP());
  flashLedBlocking(5);
}

void flashLedBlocking(int count)
{
  for (int i = 0; i < count; i++)
  {
    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);
    delay(50);
  }
}

void blinkLed(int highMillis) { state.nextLedLowMillis = millis() + highMillis; }

void updateLed()
{
  if (millis() >= state.nextLedLowMillis)
  {
    digitalWrite(LED_PIN, LOW);
  }
  else
  {
    digitalWrite(LED_PIN, HIGH);
  }
}

float readVoltage()
{
  float volts = float(analogRead(VOLTAGE_PIN));
  // Serial.printf("Volts: %f\n", volts);
  return volts;
}

float readCurrent() { return analogRead(CURRENT_PIN); }

float avg(const std::vector<float> &readings)
{
  if (readings.empty())
  {
    return 0.0;
  }
  float sum = 0.0;
  for (float reading : readings)
  {
    sum += reading;
  }
  Serial.printf("Sum: %f, Count: %zu\n", sum, readings.size());
  return sum / readings.size();
}
