#pragma once
#include <Arduino.h>
#include <Preferences.h>

Preferences preferences;
// char newNodeID[16];

#define Change_NODE_ID 0

#if Change_NODE_ID
    char newNodeID[16] = "1225213";
#endif


// ================= CONFIG =================
char nodeID[16];     // Global, mutable buffer

int hb_interval; 
unsigned long HB_INTERVAL;
unsigned long lastHeartbeat = 0;

bool isRepeater;
// #define MAX_FWDS 50
// #define MAX_HOPS 5
int MAX_FWDS;
int MAX_HOPS;

#define DEBUG_MODE true
#define DEBUG_PRINT(x)  if (DEBUG_MODE) { Serial.print(x); }
#define DEBUG_PRINTF(x) if (DEBUG_MODE) { Serial.printf(x); }
#define DEBUG_PRINTLN(x) if (DEBUG_MODE) { Serial.println(x); }
