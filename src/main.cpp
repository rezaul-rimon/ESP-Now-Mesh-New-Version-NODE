#include <Arduino.h>
#include "config.h"
#include "mesh_node.h"
#include "led.h"


// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  fastLED_setup();

  preferences.begin("device_config", false);

  #if Change_NODE_ID
    String newIDStr(newNodeID);
    if(newIDStr.length() > 0 && newIDStr.length() < sizeof(nodeID)) {
        preferences.putString("node_id", newIDStr);
        Serial.printf("Node ID set to: %s\n", newIDStr.c_str());
    }
  #endif

  String node_id = preferences.getString("node_id", "NODE1");
  isRepeater = preferences.getBool("is_repeater", false);
  MAX_FWDS = preferences.getInt("max_fwds", 50);
  MAX_HOPS = preferences.getInt("max_hops", 5);
  hb_interval = preferences.getInt("hb_interval", 5);
  HB_INTERVAL = hb_interval * 60 * 1000;

  preferences.end();

  strncpy(nodeID, node_id.c_str(), sizeof(nodeID));
  nodeID[sizeof(nodeID)-1] = '\0';

  mesh_node_setup();

  Serial.printf("✅ Node %s ready | repeater=%d | hb_interval=%d | max_fwds=%d | max_hops=%d \n", nodeID, isRepeater, hb_interval, MAX_FWDS, MAX_HOPS);
}

// ================= LOOP =================
void loop() {

  if(millis() - lastHeartbeat > HB_INTERVAL) {
    lastHeartbeat = millis();

    String heartbeat =
        String(nodeID) + "," +
        "gw0" + "," +
        "heartbeat" + "," +
        String(MSG_HEARTBEAT) + "," +
        String(random(10000, 99999)) + "," +
        String(nodeID) + "," +
        "0";

    if(useEncryption) {
        String encHb = encryptSimple(heartbeat, enckey);
        esp_now_send(broadcastAddress, (uint8_t *)encHb.c_str(), encHb.length());
        DEBUG_PRINTLN("📤 Heartbeat Sent: " + encHb);
        DEBUG_PRINTLN("📤 Original Heartbeat: " + decryptSimple(encHb, enckey));
    }
    else {
        esp_now_send(broadcastAddress, (uint8_t *)heartbeat.c_str(), heartbeat.length());
        DEBUG_PRINTLN("📤 Heartbeat Sent: " + heartbeat);
    }
  }
  delay(100);
}
