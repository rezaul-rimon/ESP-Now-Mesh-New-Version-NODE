#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <FastLED.h>

//============== Function Prototyping ==========
void publisshHeartBeat();
void publishSensorData();
void suspendAllTasks();
void startLocalOTA();
void LocalOtaTask(void *pvParameters);
//=================================================

#define USE_SENSOR
#define USE_LDR
#define USE_CT
#define USE_SHT_TMP

bool otaMode = false;
unsigned long otaStartTime = 0;
#define OTA_TIMEOUT_MS (15UL * 60UL * 1000UL)

Preferences preferences;

bool IS_LDR_REVERSE;
float CT_CALIB_FACTOR;

bool onOffByLDR;
int ldrLowValue;
int ldrHighValue;

#define Change_NODE_ID 0

#if Change_NODE_ID
    char newNodeID[16] = "1225369"; // Set new Node ID if needed
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

TaskHandle_t LocalOtaTaskHandle = NULL;
#define LocalOtaTask_PRIORITY 3
#define LocalOtaTask_STACK 8 * 1024

TaskHandle_t ledTaskHandle = NULL;
#define LED_TASK_PRIORITY 1
#define LED_TASK_STACK 2 * 1024

TaskHandle_t mainTaskHandle = NULL;
#define MAIN_TASK_PRIORITY 1
#define MAIN_TASK_STACK 8 * 1024

#define DEBUG_MODE true
#define DEBUG_PRINT(x)  if (DEBUG_MODE) { Serial.print(x); }
#define DEBUG_PRINTF(x) if (DEBUG_MODE) { Serial.printf(x); }
#define DEBUG_PRINTLN(x) if (DEBUG_MODE) { Serial.println(x); }



