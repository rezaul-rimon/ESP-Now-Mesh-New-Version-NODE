#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"
#include "EmonLib.h"                   // Include Emon Library
#include "NTC.h"

// #define USE_SENSOR

#if defined(USE_SENSOR)
    #define USE_SHT_TMP
    #define USE_CT
    #define USE_LDR
    #define USE_NTC_SENSOR
#endif

#if defined(USE_SHT_TMP)
    Adafruit_SHT31 sht = Adafruit_SHT31();
    bool shtInitialized = false;
    #define SHT3X_ADDR 0x44
#endif

#if defined(USE_CT)
    EnergyMonitor emon1;
    #define CT_PIN 34
#endif

#if defined(USE_LDR)
    int ldrValue = 0;
    #define LDR_PIN 32
#endif

#if defined(USE_NTC_SENSOR)
    // #define NTC_PIN 36
    NTC ntcSensor(36); // GPIO36 analog pin;
#endif

#if defined(USE_SHT_TMP)
    String readTemperature() {
        if (!shtInitialized)
            return "NA/NA";

        float temp = sht.readTemperature();
        float hum = sht.readHumidity();

        if (!isnan(temp) && !isnan(hum)) {
            // return String(temp, 2) + "/" + String(hum, 2);
            return String(temp, 1);
        }
        else {
            return "N/A";
        }
    }
#endif

#if defined(USE_SHT_TMP)
    void sht3x_sensor_setup() {
        // First attempt
        if (sht.begin(SHT3X_ADDR)) {
            shtInitialized = true;
            Serial.println("✅ SHT3x sensor initialized!");
            // leds[0] = CRGB::Green; FastLED.show();
            // delay(1000);
            // leds[0] = CRGB::Black; FastLED.show();
        }
        else {
            Serial.println("⏳ SHT3x not found, will retry in loop.");
        }
    }
#endif

void ct_setup(){
    #if defined(USE_CT)
        Serial.println("CT initialized done!");
        emon1.current(CT_PIN, CT_CALIB_FACTOR);
    #endif
}

void ldr_setup(){
    #if defined(USE_LDR)
        Serial.println("LDR initialized done!");
        pinMode(LDR_PIN, INPUT);
    #endif
}