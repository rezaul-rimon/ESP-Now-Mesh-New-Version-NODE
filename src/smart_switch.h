#pragma once
#include <config.h>
#include "led.h"
#include <Arduino.h>

#define SW_PIN1 14  
#define SW_PIN2 27  
#define SW_PIN3 26 
#define SW_PIN4 25

void smart_switch_setup() {
   // Setup switch pins
    pinMode(SW_PIN1, OUTPUT);
    pinMode(SW_PIN2, OUTPUT);
    pinMode(SW_PIN3, OUTPUT);
    pinMode(SW_PIN4, OUTPUT);

    // Initialize switches to OFF
    digitalWrite(SW_PIN1, LOW);
    digitalWrite(SW_PIN2, LOW);
    digitalWrite(SW_PIN3, LOW);
    digitalWrite(SW_PIN4, LOW);

    // Restore switch states from Preferences
    preferences.begin("switches", false);  // Open Preferences

    digitalWrite(SW_PIN1, preferences.getBool("sw1", true)); // Default: OFF
    digitalWrite(SW_PIN2, preferences.getBool("sw2", true));
    digitalWrite(SW_PIN3, preferences.getBool("sw3", true));
    digitalWrite(SW_PIN4, preferences.getBool("sw4", true));

    preferences.end();
}

void handleSwitches(String message) {

  preferences.begin("switches", false);  // Open Preferences storage

  if (message == "sw1:1") {
    // DEBUG_PRINTLN("Switch-1: On");
    // digitalWrite(SW_PIN1, HIGH);
    // preferences.putBool("sw1", true);  // Save state

    // sendLedCommand(LED_SWITCH_SINGLE_ON);
    DEBUG_PRINTLN("Switch-1234: On");
    digitalWrite(SW_PIN1, HIGH);
    digitalWrite(SW_PIN2, HIGH);
    digitalWrite(SW_PIN3, HIGH);
    digitalWrite(SW_PIN4, HIGH);

    preferences.putBool("sw1", true);
    preferences.putBool("sw2", true);
    preferences.putBool("sw3", true);
    preferences.putBool("sw4", true);

    sendLedCommand(LED_SWITCH_ALL_ON);
  } 
  else if (message == "sw1:0") {
    //   DEBUG_PRINTLN("Switch-1: Off");
    //   digitalWrite(SW_PIN1, LOW);
    //   preferences.putBool("sw1", false); 

    //   sendLedCommand(LED_SWITCH_SINGLE_OFF);

    DEBUG_PRINTLN("Switch-1234: Off");
    digitalWrite(SW_PIN1, LOW);
    digitalWrite(SW_PIN2, LOW);
    digitalWrite(SW_PIN3, LOW);
    digitalWrite(SW_PIN4, LOW);

    preferences.putBool("sw1", false);
    preferences.putBool("sw2", false);
    preferences.putBool("sw3", false);
    preferences.putBool("sw4", false);

    sendLedCommand(LED_SWITCH_ALL_OFF);
  }

  if (message == "sw2:1") {
      DEBUG_PRINTLN("Switch-2: On");
      digitalWrite(SW_PIN2, HIGH);
      preferences.putBool("sw2", true);
      sendLedCommand(LED_SWITCH_SINGLE_ON);
  } 
  else if (message == "sw2:0") {
      DEBUG_PRINTLN("Switch-2: Off");
      digitalWrite(SW_PIN2, LOW);
      preferences.putBool("sw2", false);
      sendLedCommand(LED_SWITCH_SINGLE_OFF);
  }

  if (message == "sw3:1") {
      DEBUG_PRINTLN("Switch-3: On");
      digitalWrite(SW_PIN3, HIGH);
      preferences.putBool("sw3", true);
      sendLedCommand(LED_SWITCH_SINGLE_ON);
  } 
  else if (message == "sw3:0") {
      DEBUG_PRINTLN("Switch-3: Off");
      digitalWrite(SW_PIN3, LOW);
      preferences.putBool("sw3", false);
      sendLedCommand(LED_SWITCH_SINGLE_OFF);
  }

  if (message == "sw4:1") {
      DEBUG_PRINTLN("Switch-4: On");
      digitalWrite(SW_PIN4, HIGH);
      preferences.putBool("sw4", true);
      sendLedCommand(LED_SWITCH_SINGLE_ON);
  } 
  else if (message == "sw4:0") {
      DEBUG_PRINTLN("Switch-4: Off");
      digitalWrite(SW_PIN4, LOW);
      preferences.putBool("sw4", false);
      sendLedCommand(LED_SWITCH_SINGLE_OFF);
  }

  //Handle All Switches Together
  if (message == "sw1234:1") {
    DEBUG_PRINTLN("Switch-1234: On");
    digitalWrite(SW_PIN1, HIGH);
    digitalWrite(SW_PIN2, HIGH);
    digitalWrite(SW_PIN3, HIGH);
    digitalWrite(SW_PIN4, HIGH);

    preferences.putBool("sw1", true);
    preferences.putBool("sw2", true);
    preferences.putBool("sw3", true);
    preferences.putBool("sw4", true);

    sendLedCommand(LED_SWITCH_ALL_ON);
  } 
  else if (message == "sw1234:0") {
    DEBUG_PRINTLN("Switch-1234: Off");
    digitalWrite(SW_PIN1, LOW);
    digitalWrite(SW_PIN2, LOW);
    digitalWrite(SW_PIN3, LOW);
    digitalWrite(SW_PIN4, LOW);

    preferences.putBool("sw1", false);
    preferences.putBool("sw2", false);
    preferences.putBool("sw3", false);
    preferences.putBool("sw4", false);

    sendLedCommand(LED_SWITCH_ALL_OFF);
  }
  preferences.end();  // Close Preferences storage
  
}