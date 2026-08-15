#include <Arduino.h>
#include "config.h"
#include "mesh_node.h"
#include "led.h"
#include "smart_switch.h"
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

  preferences.begin("switches", false);
  bool sw1 = preferences.getBool("sw1", true);

  snprintf(
    command,
    sizeof(command),
    "heartbeat/R:%s/sw1:%s",
    isRepeater ? "1" : "0",
    sw1 ? "1" : "0"
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
  #ifdef USE_DS18B20

    char command[128];

    // Read all sensors simultaneously
    sensors.requestTemperatures();

    for (int i = 0; i < sensorCount; i++) {
      float temperature = sensors.getTempC(sensorAddress[i]);

      // Skip disconnected sensor
      if (temperature == DEVICE_DISCONNECTED_C) {
        Serial.print("Sensor ");
        Serial.print(i);
        Serial.println(" disconnected.");
        continue;
      }

      // Sensor ID
      String id = addressToString(sensorAddress[i]);

      Serial.print(id);
      Serial.print(",");
      Serial.println(temperature);

      // Build command:
      // 28FF641E7B1603A5/27.56
      snprintf(
          command,
          sizeof(command),
          "%s/%.2f",
          id.c_str(),
          temperature);

      // Generate Message ID
      char msg_id[8];
      snprintf(msg_id, sizeof(msg_id), "%04X", esp_random() & 0xFFFF);

      // Final ESP-NOW Payload
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
          0);

      // Send
      if (useEncryption) {
        char encPayload[ESPNOW_MAX_MSG_LEN + 1];

        encryptSimple(nodePayload, encPayload, enckey);

        esp_now_send(
            broadcastAddress,
            (uint8_t *)encPayload,
            strlen(encPayload));

        DEBUG_PRINTLN(encPayload);
      }
      else {
        esp_now_send(
          broadcastAddress,
          (uint8_t *)nodePayload,
          strlen(nodePayload));

        DEBUG_PRINTLN(nodePayload);
      }

      // Small delay to avoid flooding ESP-NOW
      delay(10);
    }

    Serial.println("----------------------------");

  #endif

    sendLedCommand(LED_HEARTBEAT);
}

//==========================================================//
//===================== Main Task ==========================//
//==========================================================//
void mainTask(void *parameter) {
  // void publisshHeartBeat();
  HB_INTERVAL = HB_INTERVAL + random(0, 15000); // Randomize heartbeat interval between 0-15 seconds for testing

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

      vTaskDelay(pdMS_TO_TICKS(100));

      publishSensorData();

      //===============================================//

      if (digitalRead(0) == HIGH) {
        isButtonPressed = false;
      }
    }
    //===================================================//

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
  smart_switch_setup();

  preferences.begin("device_config", false);

  #if Change_NODE_ID
    String newIDStr(newNodeID);
    if (newIDStr.length() > 0 && newIDStr.length() < sizeof(nodeID))
    {
      preferences.putString("node_id", newIDStr);
      Serial.printf("Node ID set to: %s\n", newIDStr.c_str());
    }
  #endif

  String node_id = preferences.getString("node_id", "AERATOR_NODE");
  isRepeater = preferences.getBool("is_repeater", false);
  MAX_FWDS = preferences.getInt("max_fwds", 500);
  MAX_HOPS = preferences.getInt("max_hops", 10);
  hb_interval = preferences.getInt("hb_interval", 5);
  HB_INTERVAL = hb_interval * 60 * 1000;
  // preferences.putBool("use_encryption", false);
  useEncryption = preferences.getBool("use_encryption", false);


  preferences.end();

  strncpy(nodeID, node_id.c_str(), sizeof(nodeID));
  nodeID[sizeof(nodeID) - 1] = '\0';

  // ds18b20_setup();
  mesh_node_setup();

  Serial.printf("✅ Node %s ready | repeater=%d | hb_interval=%d \n max_fwds=%d | max_hops=%d | useEncryption=%d \n", nodeID, isRepeater, hb_interval, MAX_FWDS, MAX_HOPS, useEncryption);

  xTaskCreatePinnedToCore(mainTask, "MainTask", MAIN_TASK_STACK, NULL, MAIN_TASK_PRIORITY, &mainTaskHandle, 0);
  // xTaskCreatePinnedToCore(LocalOtaTask,"LocalOTA",LocalOtaTask_STACK,NULL,LocalOtaTask_PRIORITY,&LocalOtaTaskHandle,1);
}

// ================= LOOP =================
void loop() {
  vTaskDelay(pdMS_TO_TICKS(100));
}