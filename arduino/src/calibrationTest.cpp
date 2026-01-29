// // Adapted from https://github.com/G6EJD/ESP32-ADC-Accuracy-Improvement/tree/main

// #include <Arduino.h>
// #include <vector>
// #include "esp_adc_cal.h"

// #define ADC_PIN     32

// float ReadVoltage(byte ADC_Pin);
// std::vector<float> unadjusted_voltage_readings;
// std::vector<float> adjusted_voltage_readings;
// unsigned long next_millis = 0;

// void setup() {
//   Serial.begin(115200);
// }

// void loop() {
//   // Collect readings for 100 ms
//   unadjusted_voltage_readings.clear();
//   adjusted_voltage_readings.clear();
//   while (millis() < next_millis) {
//     unadjusted_voltage_readings.push_back(analogRead(ADC_PIN) / 4095.0 * 3.3);
//     adjusted_voltage_readings.push_back(ReadVoltage(ADC_PIN));
//   }

//   // Calculate averages
//   float unadjusted_voltage = 0.0;
//   float adjusted_voltage = 0.0;
//   for (float reading : unadjusted_voltage_readings) {
//     unadjusted_voltage += reading;
//   }
//   for (float reading : adjusted_voltage_readings) {
//     adjusted_voltage += reading;
//   }
//   unadjusted_voltage /= unadjusted_voltage_readings.size();
//   adjusted_voltage /= adjusted_voltage_readings.size();
//   float error = adjusted_voltage != 0 ? abs(unadjusted_voltage - adjusted_voltage) / adjusted_voltage * 100 : 0.0;

//   // Print
//   Serial.println("   Adjusted Voltage = " + String(adjusted_voltage, 3) + "V");
//   Serial.println("Un-adjusted Voltage = " + String(unadjusted_voltage, 3) + "V");
//   Serial.println(String(error) + "% error");
//   Serial.println();
//   next_millis = millis() + 100;
// }

// float ReadVoltage(byte ADC_Pin) {
//   float calibration  = 1.000; // Adjust for ultimate accuracy when input is measured using an accurate DVM, if reading too high then use e.g. 0.99, too low use 1.01
//   float vref = 1100;
//   esp_adc_cal_characteristics_t adc_chars;
//   esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 1100, &adc_chars);
//   vref = adc_chars.vref; // Obtain the device ADC reference voltage
//   return (analogRead(ADC_Pin) / 4095.0) * 3.3 * (1100 / vref) * calibration;  // ESP by design reference voltage in mV
// }

// // The esp_adc_cal/include/esp_adc_cal.h API provides functions to correct for differences
// // in measured voltages caused by variation of ADC reference voltages (Vref) between chips.
// // Per design the ADC reference voltage is 1100 mV, however the true reference voltage can
// // range from 1000 mV to 1200 mV amongst different ESP32's