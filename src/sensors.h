#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"
#include "EmonLib.h"                   // Include Emon Library
#include "NTC.h"

#define USE_SHT
#define USE_CT
#define USE_LDR
#define USE_NTC_SENSOR

#if defined(USE_SHT)
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

#if defined(USE_SHT)
String readTemperature()
    {
        if (!shtInitialized)
            return "NA/NA";

        float temp = sht.readTemperature();
        float hum = sht.readHumidity();

        if (!isnan(temp) && !isnan(hum))
        {
            // return String(temp, 2) + "/" + String(hum, 2);
            return String(temp, 1);
        }
        else
        {
            return "N/A";
        }
    }
#endif

#if defined(USE_SHT)
void sht3x_sensor_setup()
    {
        // First attempt
        if (sht.begin(SHT3X_ADDR))
        {
            shtInitialized = true;
            Serial.println("✅ SHT3x sensor initialized!");
            // leds[0] = CRGB::Green; FastLED.show();
            // delay(1000);
            // leds[0] = CRGB::Black; FastLED.show();
        }
        else
        {
            Serial.println("⏳ SHT3x not found, will retry in loop.");
        }
    }
#endif

void ct_setup(){
    Serial.println("CT initialized done!");
    emon1.current(CT_PIN, CT_CALIB_FACTOR);
}

void ldr_setup(){
    pinMode(LDR_PIN, INPUT);
}