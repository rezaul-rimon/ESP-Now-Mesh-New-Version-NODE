#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <FastLED.h>

Preferences preferences;
// #define Fast_LED 1
// char newNodeID[16];

#define Change_NODE_ID 0

#if Change_NODE_ID
    char newNodeID[16] = "1225021"; // Set new Node ID if needed
#endif
const char* MasterID = "9999999"; // Set Master ID if needed

bool isButtonPressed = false; // Global flag for button press state


// ================= CONFIG =================
char nodeID[16];     // Global, mutable buffer

int hb_interval; 
unsigned long HB_INTERVAL;
unsigned long lastHeartbeat = 0;

bool isRepeater;
int MAX_FWDS;
int MAX_HOPS;

#define DEBUG_MODE true
#define DEBUG_PRINT(x)  if (DEBUG_MODE) { Serial.print(x); }
#define DEBUG_PRINTF(x) if (DEBUG_MODE) { Serial.printf(x); }
#define DEBUG_PRINTLN(x) if (DEBUG_MODE) { Serial.println(x); }

// ================= LED =================
// #define Fast_LED 1

// #if Fast_LED
//     #define LED_PIN 4
//     #define NUM_LEDS 1
//     CRGB leds[NUM_LEDS];
// #endif
