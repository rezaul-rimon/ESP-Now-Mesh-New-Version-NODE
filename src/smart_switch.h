#pragma once
#include <config.h>
#include "led.h"
#include <Arduino.h>

#define SW_PIN1 21

#define ON HIGH
#define OFF LOW

void smart_switch_setup() {
   // Setup switch pins
    pinMode(SW_PIN1, OUTPUT);

    // Initialize switches to OFF
    digitalWrite(SW_PIN1, OFF);

    // Restore switch states from Preferences
    preferences.begin("switches", false);  // Open Preferences
    digitalWrite(SW_PIN1, preferences.getBool("sw1", true)); // Default: ON
    preferences.end();
}

void handleSwitches(String message) {

  preferences.begin("switches", false);  // Open Preferences storage

  if (message == "sw1:1") {
    DEBUG_PRINTLN("Switch-1: On");
    digitalWrite(SW_PIN1, ON);
    preferences.putBool("sw1", true);  // Save state
    sendLedCommand(LED_SWITCH_SINGLE_ON);
  } 
  else if (message == "sw1:0") {
      DEBUG_PRINTLN("Switch-1: Off");
      digitalWrite(SW_PIN1, OFF);
      preferences.putBool("sw1", false); 
      sendLedCommand(LED_SWITCH_SINGLE_OFF);
  }

  preferences.end();  // Close Preferences storage
  
}