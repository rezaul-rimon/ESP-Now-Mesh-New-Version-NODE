#include <Arduino.h>
#include "config.h"
#include "mesh_node.h"
#include "led.h"
#include "smart_switch.h"
#include "sensors.h"
#include <WebServer.h>
#include <Update.h>

WebServer server(80);


// TaskHandle_t mainTaskHandle = NULL;
// #define MAIN_TASK_PRIORITY 1
// #define MAIN_TASK_STACK 8 * 1024

// TaskHandle_t LocalOtaTaskHandle = NULL;
// #define LocalOtaTask_PRIORITY 3
// #define LocalOtaTask_STACK 8 * 1024

//=============== PUBLISH HEARTBEAT =================
void publisshHeartBeat() {
  char command[48];

  snprintf(
    command,
    sizeof(command),
    "heartbeat/R:%s",
    isRepeater ? "1" : "0"
  );

  char nodePayload[ESPNOW_MAX_MSG_LEN + 1];

  snprintf(
    nodePayload,
    sizeof(nodePayload),
    "%s,%s,%s,%d,%s,%s,%d",
    nodeID,
    "gw0",
    command,
    MSG_HB,
    generateMessageID(),   // ⚠️ see note below
    nodeID,
    0
  );

  if (useEncryption) {
    char encHb[ESPNOW_MAX_MSG_LEN + 1];

    encryptSimple(nodePayload, encHb, enckey);

    esp_now_send(
      broadcastAddress,
      (uint8_t*)encHb,
      strlen(encHb)
    );

    DEBUG_PRINTLN(encHb);
  }
  else {
    esp_now_send(
      broadcastAddress,
      (uint8_t*)nodePayload,
      strlen(nodePayload)
    );

    DEBUG_PRINTLN("HeartBeat: " + String(nodePayload));
    sendLedCommand(LED_HEARTBEAT);
  }
}

// ================= PUBLISH SENSOR DATA =================
void publishSensorData() {
  float temperature = ntcSensor.readTemperature();

  const int samples = 10;

  double avgIrms = 0;
  double avgWatt = 0;
  float avgLdr = 0;
  float avgLightIntensity = 0;

  for (int i = 0; i < samples; i++)
  {
      double Irms = emon1.calcIrms(1480);
      float watt = 230.0 * Irms;

      avgIrms += Irms;
      avgWatt += watt;

      int ldr = analogRead(LDR_PIN);

      if (IS_LDR_REVERSE)
      {
          ldr = 4095 - ldr;
      }

      float light_intensity = ldr / 40.95;

      avgLdr += ldr;
      avgLightIntensity += light_intensity;

      vTaskDelay(pdMS_TO_TICKS(500));
  }

  avgIrms /= samples;
  avgWatt /= samples;
  avgLdr /= samples;
  avgLightIntensity /= samples;

  DEBUG_PRINTLN("T: " + String(temperature));
  DEBUG_PRINTLN("I: " + String(avgIrms));
  DEBUG_PRINTLN("W: " + String(avgWatt));
  DEBUG_PRINTLN("LDR: " + String(avgLdr));
  DEBUG_PRINTLN("Light: " + String(avgLightIntensity));

  preferences.begin("switches", false);
  bool sw1 = preferences.getBool("sw1", true);
  preferences.end();

  // ================= BUILD COMMAND (NO STRING) =================
  char command[128];

  snprintf(
      command,
      sizeof(command),
      "sd/W:%.0f/L:%.0f/T:%.1f/Tg:0/LDS:%d/sw1:%d",
      avgWatt,
      avgLightIntensity,
      temperature,
      onOffByLDR ? 1 : 0,
      sw1 ? 1 : 0
  );

  // ================= MESSAGE ID (NO String function) =================
  char msg_id[8];
  snprintf(msg_id, sizeof(msg_id), "%04X", esp_random() & 0xFFFF);

  // ================= BUILD FINAL PACKET =================
  char nodePayload[ESPNOW_MAX_MSG_LEN + 1];

  snprintf(
      nodePayload,
      sizeof(nodePayload),
      "%s,%s,%s,%d,%s,%s,%d",
      nodeID,
      "gw0",
      command,
      MSG_SD,
      msg_id,
      nodeID,
      0
  );

  // ================= SEND =================
  if (useEncryption)
  {
      char encPayload[ESPNOW_MAX_MSG_LEN + 1];

      encryptSimple(nodePayload, encPayload, enckey);

      esp_now_send(
          broadcastAddress,
          (uint8_t *)encPayload,
          strlen(encPayload)
      );

      DEBUG_PRINTLN(encPayload);
  }
  else
  {
      esp_now_send(
          broadcastAddress,
          (uint8_t *)nodePayload,
          strlen(nodePayload)
      );

      DEBUG_PRINTLN("SensorData: " + String(nodePayload));
  }

  sendLedCommand(LED_HEARTBEAT);
}

//==========================================================//
//===================== Main Task ==========================//
//==========================================================//
void mainTask(void *parameter) {
  // void publisshHeartBeat();
  HB_INTERVAL = HB_INTERVAL + random(0, 15000); // Randomize heartbeat interval between 3-7 seconds for testing

  while (1)
  {

    if ((millis() - lastHeartbeat > HB_INTERVAL) || (isButtonPressed == false && digitalRead(0) == LOW))
    {

      lastHeartbeat = millis();

      if (digitalRead(0) == LOW) {
        isButtonPressed = true;
      }

      //===============================================//
      publisshHeartBeat();

      #if defined(USE_SHT_TMP)
        // 🔴 Handle sensor reinitialization & LED blinking if not ready
        if (!shtInitialized) {
          static unsigned long lastAttempt = 0;
          static unsigned long lastBlink = 0;
          static bool ledOn = false;

          // 🔄 Retry sensor init every 10 seconds
          if (millis() - lastAttempt > 10000)
          {
            DEBUG_PRINTLN("🔄 Retrying SHT3x init...");
            if (sht.begin(0x44))
            {
              shtInitialized = true;
              DEBUG_PRINTLN("✅ SHT3x initialized during loop.");
              leds[0] = CRGB::Green;
              FastLED.show();
              delay(1000);
              leds[0] = CRGB::Black;
              FastLED.show();
            }
            lastAttempt = millis();
          }
          // 🔴 Blink red LED every 500ms
        }
      #endif

      vTaskDelay(pdMS_TO_TICKS(100));

      publishSensorData();

      //===============================================//

      if (digitalRead(0) == HIGH) {
        isButtonPressed = false;
      }
    }
    //===================================================//

    static bool lastState = false;
    bool currentState;

    if (onOffByLDR) {
        int ldr = analogRead(LDR_PIN);
        if(IS_LDR_REVERSE == true){
          ldr = 4095 - ldr;
        }

        if (ldr < ldrLowValue)
        {
            currentState = true;
        }
        else if (ldr > ldrHighValue)
        {
            currentState = false;
        }
        else
        {
            currentState = lastState;
        }

        // State changed?
        if (currentState != lastState)
        {
            if (currentState)
            {
              DEBUG_PRINTLN("LDR: "+String(ldr));
              DEBUG_PRINTLN("LDR -> ON");
              handleSwitches("sw1:1");
            }
            else
            {
              DEBUG_PRINTLN("LDR: "+String(ldr));
              DEBUG_PRINTLN("LDR -> OFF");
              handleSwitches("sw1:0");
            }

            // publishSensorData();

            // Update AFTER action
            lastState = currentState;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

//================== SUSPEND ALL TASKS =================
void suspendAllTasks() {
  if (mainTaskHandle != NULL) {
    vTaskSuspend(mainTaskHandle);
  }
  if (EspNowOnReceiveTaskHandle != NULL) {
    vTaskSuspend(EspNowOnReceiveTaskHandle);
  }
  //NO need to suspend led task and Local OTA Task
}

//================== LOCAL OTA UPDATE =================
void startLocalOTA() {
    suspendAllTasks();
    server.on("/", HTTP_GET, []()
    {
        server.send(
            200,
            "text/html",
            "<form method='POST' action='/update' enctype='multipart/form-data'>"
            "<input type='file' name='update'>"
            "<input type='submit' value='Upload'>"
            "</form>"
        );
    });

    server.on(
        "/update",
        HTTP_POST,
        []()
        {
            server.send(200, "text/plain", "Update Success. Rebooting...");
            delay(1000);
            ESP.restart();
        },
        []()
        {
            HTTPUpload& upload = server.upload();

            if(upload.status == UPLOAD_FILE_START)
            {
                Update.begin(UPDATE_SIZE_UNKNOWN);
            }
            else if(upload.status == UPLOAD_FILE_WRITE)
            {
                Update.write(upload.buf, upload.currentSize);
            }
            else if(upload.status == UPLOAD_FILE_END)
            {
                Update.end(true);
            }
        }
    );

    server.begin();
}

//================== LOCAL OTA TASK =================
void LocalOtaTask(void *pvParameters) {
    bool otaStarted = false;
    DEBUG_PRINTLN("Local OTA Task Started");

    while(true)
    {
        if(otaMode)
        {
            if(!otaStarted)
            {
                otaStarted = true;

                DEBUG_PRINTLN("Starting OTA AP...");

                WiFi.mode(WIFI_AP);

                String ssid = "LP_" + String(nodeID);

                WiFi.softAP(
                    ssid.c_str(),
                    "dmabd987"
                );

                DEBUG_PRINT("OTA IP: ");
                DEBUG_PRINTLN(WiFi.softAPIP());

                // Start ElegantOTA / WebServer here
                startLocalOTA();
            }

            // Handle OTA requests
            server.handleClient();

            // Timeout
            if(millis() - otaStartTime > OTA_TIMEOUT_MS)
            {
                DEBUG_PRINTLN("OTA Timeout");

                otaMode = false;

                ESP.restart();
            }
        }
        else
        {
            otaStarted = false;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  FastLED_setup();

  preferences.begin("device_config", false);

  #if Change_NODE_ID
    String newIDStr(newNodeID);
    if (newIDStr.length() > 0 && newIDStr.length() < sizeof(nodeID))
    {
      preferences.putString("node_id", newIDStr);
      Serial.printf("Node ID set to: %s\n", newIDStr.c_str());
    }
  #endif

  String node_id = preferences.getString("node_id", "NODE1");
  isRepeater = preferences.getBool("is_repeater", true);
  MAX_FWDS = preferences.getInt("max_fwds", 500);
  MAX_HOPS = preferences.getInt("max_hops", 10);
  hb_interval = preferences.getInt("hb_interval", 5);
  HB_INTERVAL = hb_interval * 60 * 1000;
  // preferences.putBool("use_encryption", false);
  useEncryption = preferences.getBool("use_encryption", false);

  onOffByLDR = preferences.getBool("lds", false);
  ldrLowValue = preferences.getInt("ldsLow", 40*40);
  ldrHighValue = preferences.getInt("ldsHigh", 80*40);
  IS_LDR_REVERSE = preferences.getBool("ldrRev", false);
  CT_CALIB_FACTOR = preferences.getFloat("ctRatio", 1.25);


  preferences.end();

  strncpy(nodeID, node_id.c_str(), sizeof(nodeID));
  nodeID[sizeof(nodeID) - 1] = '\0';

  smart_switch_setup();
  mesh_node_setup();
  // sht3x_sensor_setup();
  ct_setup();
  ldr_setup();

  Serial.printf("✅ Node %s ready | repeater=%d | hb_interval=%d \n max_fwds=%d | max_hops=%d | useEncryption=%d \n LDS=%d | LDR_High=%d | LDR_Low=%d \n LDR Reverse=%d | CT_Ratio=%f\n", nodeID, isRepeater, hb_interval, MAX_FWDS, MAX_HOPS, useEncryption, onOffByLDR, ldrHighValue, ldrLowValue, IS_LDR_REVERSE, CT_CALIB_FACTOR);

  xTaskCreatePinnedToCore(mainTask, "MainTask", MAIN_TASK_STACK, NULL, MAIN_TASK_PRIORITY, &mainTaskHandle, 0);
  // xTaskCreatePinnedToCore(LocalOtaTask,"LocalOTA",LocalOtaTask_STACK,NULL,LocalOtaTask_PRIORITY,&LocalOtaTaskHandle,1);
}

// ================= LOOP =================
void loop() {
  vTaskDelay(pdMS_TO_TICKS(100));
}