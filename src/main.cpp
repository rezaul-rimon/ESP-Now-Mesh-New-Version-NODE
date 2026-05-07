#include <Arduino.h>
#include "config.h"
#include "mesh_node.h"
#include "led.h"
#include "smart_switch.h"

TaskHandle_t mainTaskHandle = NULL;

#define MAIN_TASK_PRIORITY 1
#define MAIN_TASK_STACK 16 * 1024

void mainTask(void* parameter) {
  HB_INTERVAL = HB_INTERVAL + random(0, 900); // Randomize heartbeat interval between 3-7 seconds for testing
  
  while(1){

    if((millis() - lastHeartbeat > HB_INTERVAL) || (isButtonPressed == false && digitalRead(0) == LOW)) {
    
      lastHeartbeat = millis();

    if(digitalRead(0)==LOW) {
      isButtonPressed = true;
    }

    Message hbmsg;
    hbmsg.sender_id = nodeID;
    hbmsg.receiver_id = "gw0";
    hbmsg.command = "heartbeat/R:" + String(isRepeater ? "1" : "0");
    hbmsg.type = MSG_HB;
    hbmsg.msg_id = generateMessageID();
    hbmsg.last_hop = nodeID;
    hbmsg.hop_count = 0;

    String nodePayload =
        hbmsg.sender_id + "," +
        hbmsg.receiver_id + "," +
        hbmsg.command + "," +
        String(hbmsg.type) + "," +
        hbmsg.msg_id + "," +
        hbmsg.last_hop + "," +
        String(hbmsg.hop_count);

    if(useEncryption) {
        String encHb = encryptSimple(nodePayload, enckey);
        esp_now_send(broadcastAddress, (uint8_t *)encHb.c_str(), encHb.length());
        DEBUG_PRINTLN("📤 Heartbeat Sent: " + encHb);
        DEBUG_PRINTLN("📤 Original Heartbeat: " + decryptSimple(encHb, enckey));
    }
    else {
        esp_now_send(broadcastAddress, (uint8_t *)nodePayload.c_str(), nodePayload.length());
        DEBUG_PRINTLN("📤 Heartbeat Sent: " + nodePayload);
    }
    sendLedCommand(LED_HEARTBEAT);

    if(digitalRead(0)==HIGH) {
      isButtonPressed = false;
    }

  }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  #if Fast_LED
    FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
    leds[0] = CRGB::Yellow;
    FastLED.show();
    delay(500);
    leds[0] = CRGB::Green;
    FastLED.show();
    delay(500);
    leds[0] = CRGB::Black;
    FastLED.show();
  #endif

  FastLED_setup();

  preferences.begin("device_config", false);

  #if Change_NODE_ID
    String newIDStr(newNodeID);
    if(newIDStr.length() > 0 && newIDStr.length() < sizeof(nodeID)) {
        preferences.putString("node_id", newIDStr);
        Serial.printf("Node ID set to: %s\n", newIDStr.c_str());
    }
  #endif

  String node_id = preferences.getString("node_id", "NODE1");
  isRepeater = preferences.getBool("is_repeater", true);
  MAX_FWDS = preferences.getInt("max_fwds", 50);
  MAX_HOPS = preferences.getInt("max_hops", 5);
  hb_interval = preferences.getInt("hb_interval", 5);
  HB_INTERVAL = hb_interval * 60 * 1000;
        // preferences.putBool("use_encryption", false);
  useEncryption = preferences.getBool("use_encryption", false);

  preferences.end();

  strncpy(nodeID, node_id.c_str(), sizeof(nodeID));
  nodeID[sizeof(nodeID)-1] = '\0';

  smart_switch_setup();
  mesh_node_setup();

  Serial.printf("✅ Node %s ready | repeater=%d | hb_interval=%d | max_fwds=%d | max_hops=%d | useEncryption=%d\n", nodeID, isRepeater, hb_interval, MAX_FWDS, MAX_HOPS, useEncryption);

  xTaskCreatePinnedToCore(mainTask, "MainTask", MAIN_TASK_STACK, NULL, MAIN_TASK_PRIORITY, &mainTaskHandle, 0);
}

// ================= LOOP =================
void loop() {
  vTaskDelay(pdMS_TO_TICKS(100));
}
