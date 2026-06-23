#pragma once

#include <config.h>
#include "smart_switch.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <deque>
#include <algorithm>
#include "led.h"

// #define ESPNOW_RX_QUEUE_SIZE 20
#define ESPNOW_MAX_MSG_LEN 80
bool needAck = false;

// ================= BROADCAST =================
uint8_t broadcastAddress[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// ================= DEDUPLICATION =================
std::deque<String> recentMsgKeys;
const size_t maxRecentIDs = 200; // Can be made configurable via Preferences

//================= ENCRYPTION =================
bool useEncryption = false;
String enckey = "dmabd987";
String encCharset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()_+=[]{}|:;<>?,./~";

// ================= MESSAGE ENUM =================
typedef enum {
    MSG_CMD = 1,
    MSG_ACK,
    MSG_HB,
    MSG_SD
} message_type_t;

//================= MESSAGE STRUCTURE =================
struct Message {
    String sender_id;
    String receiver_id;
    String command;
    message_type_t type;
    String msg_id;
    String last_hop;
    uint8_t hop_count;
};

// ================= ESP-NOW RECEIVE STRUCTURE =================
struct EspNowRxMessage
{
    int len;
    char data[ESPNOW_MAX_MSG_LEN];
};

// ================= QUEUE AND TASK HANDLES =================
TaskHandle_t EspNowOnReceiveTaskHandle = NULL;
QueueHandle_t espNowRxQueue = NULL;
#define ONRECEIVE_TASK_STACK 16 * 1024
#define ONRECEIVE_TASK_PRIORITY 2

//============= Function prototypes ================
const char* getTypeName(message_type_t type);
String generateMessageID();
String encryptSimple(String msg, String enckey);
String decryptSimple(String msg, String enckey);
bool isDuplicate(const String& sender, message_type_t type, const String& msg_id);
void rebroadcastIfNeeded(String sender, String receiver, String command,
    message_type_t type, String msg_id,
    String last_hop, int hop_count);
void onReceive(const uint8_t *mac, const uint8_t *data, int len);
void mesh_node_setup();
void EspNowOnReceiveTask(void *pvParameters);
//===========================================================//


//================= UTILITY FUNCTIONS =================
const char* getTypeName(message_type_t type) {
    switch(type) {
        case MSG_CMD: return "Command";
        case MSG_ACK: return "Acknowledgement";
        case MSG_HB: return "Heartbeat";
        case MSG_SD: return "Sendor Data";
        default: return "UNKNOWN";
    }
}

//================= MESSAGE ID GENERATION =================
String generateMessageID() {
    uint16_t randNum = esp_random() & 0xFFFF;
    char id[5];
    sprintf(id, "%04X", randNum);
    return String(id);
}

//================= ENCRYPTION FUNCTIONS =================
String encryptSimple(String msg, String enckey) {
    String out = "";

    for (int i = 0; i < msg.length(); i++)
    {
        char c = msg[i];
        int index = encCharset.indexOf(c);

        if (index == -1) {
            out += c; // keep delimiters like , / - & %
            continue;
        }

        int shift = enckey[i % enckey.length()] + i;
        int newIndex = (index + shift) % encCharset.length();
        out += encCharset[newIndex];
    }

    return out;
}

//================= DECRYPTION FUNCTIONS =================
String decryptSimple(String msg, String enckey) {
    String out = "";

    for (int i = 0; i < msg.length(); i++)
    {
        char c = msg[i];
        int index = encCharset.indexOf(c);

        if (index == -1) {
            out += c;
            continue;
        }

        int shift = enckey[i % enckey.length()] + i;
        int newIndex = index - shift;

        while (newIndex < 0)
            newIndex += encCharset.length();

        out += encCharset[newIndex];
    }

    return out;
}

// ================= DEDUP =================
bool isDuplicate(const String& sender, message_type_t type, const String& msg_id) {
    String key = sender + ":" + String(type) + ":" + msg_id;

    if (std::find(recentMsgKeys.begin(), recentMsgKeys.end(), key) != recentMsgKeys.end()) {
        return true;
    }

    recentMsgKeys.push_back(key);
    if (recentMsgKeys.size() > maxRecentIDs) {
        recentMsgKeys.pop_front();
    }

    return false;
}

// ================= REBROADCAST =================
void rebroadcastIfNeeded(String sender, String receiver, String command,
    message_type_t type, String msg_id,
    String last_hop, int hop_count) {

    //No rebroadcast if the node is not a repeater
    if (!isRepeater) return;

    //No rebroadcast if this message was sent by me
    if (sender == nodeID) return;

    //No rebroadcast if I the last hop is me (prevents loops)
    if (last_hop == nodeID) return;

    // if (type == MSG_CMD && receiver == nodeID) return;

    //No rebroadcast if th receiver is me
    if (receiver == nodeID) return;

    //No rebroadcast if then maximum hops have been reached
    if (hop_count >= MAX_HOPS) {
        DEBUG_PRINTLN("⛔ Max hops reached");
        return;
    }

    // Update routing fields
    hop_count++;
    String new_last_hop = String(nodeID);

    String newMsg =
        sender + "," +
        receiver + "," +
        command + "," +
        String(type) + "," +
        msg_id + "," +
        new_last_hop + "," +
        String(hop_count);

    delay(random(20, 70));

    if(useEncryption) {
        String encNewMsg = encryptSimple(newMsg, enckey);
        esp_now_send(broadcastAddress, (uint8_t *)encNewMsg.c_str(), encNewMsg.length());
        DEBUG_PRINTLN("🔁 Rebroadcast: " + encNewMsg);
        DEBUG_PRINTLN("🔁 Original Msg: " + decryptSimple(encNewMsg, enckey));
    } else {
        esp_now_send(broadcastAddress, (uint8_t *)newMsg.c_str(), newMsg.length());
        DEBUG_PRINTLN("🔁 Rebroadcast: " + newMsg);
    }
}

// ================= RECEIVE =================
void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
    // 1. Basic validation
    if (len <= 0 || len > ESPNOW_MAX_MSG_LEN){
        DEBUG_PRINTLN("⚠️ Packet length Exceeded: " + String(len));
        return;
    }

    EspNowRxMessage msg;

    // 2. Copy payload safely
    msg.len = len;
    memcpy(msg.data, data, len);

    // 3. Null terminate safely (IMPORTANT)
    msg.data[len] = '\0';

    // 4. Send to queue (ISR-safe)
    BaseType_t ok;

    ok = xQueueSendFromISR(
        espNowRxQueue,
        &msg,
        NULL
    );

    // 5. Optional debug (only if needed)
    if (ok != pdTRUE)
    {
        DEBUG_PRINTLN("⚠️ Queue Overflow: Failed to enqueue received message");
    }
}

// ================= MESH NODE SETUP =================
void mesh_node_setup() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        DEBUG_PRINTLN("ESP-NOW Init Failed");
        return;
    }

    espNowRxQueue = xQueueCreate(
        MAX_FWDS,
        sizeof(EspNowRxMessage)
    );

    if(espNowRxQueue == NULL)
    {
        DEBUG_PRINTLN("RX Queue Create Failed");
    }

    xTaskCreatePinnedToCore(
        EspNowOnReceiveTask,
        "EspNowRx",
        ONRECEIVE_TASK_STACK,
        NULL,
        ONRECEIVE_TASK_PRIORITY,
        &EspNowOnReceiveTaskHandle,
        1
    );

    esp_now_peer_info_t pi = {};
    memcpy(pi.peer_addr, broadcastAddress, 6);
    pi.channel = 0;
    pi.encrypt = false;
    esp_now_add_peer(&pi);

    esp_now_register_recv_cb(onReceive);
}

// ================= ESP-NOW RECEIVE TASK =================
void EspNowOnReceiveTask(void *pvParameters) {
    EspNowRxMessage rxMsg;

    while(true)
    {
        if(
            xQueueReceive(
                espNowRxQueue,
                &rxMsg,
                portMAX_DELAY
            ) == pdTRUE
        )
        {
            //Start processing the received message
            String msg = String(rxMsg.data);

            // Expect 7 fields (6 commas)
            int commas = std::count(msg.begin(), msg.end(), ',');
            if (commas != 6) {
                // DEBUG_PRINTLN("❌ Invalid packet");
                continue;
            }
            DEBUG_PRINTLN();
            DEBUG_PRINTLN("============================================");
            DEBUG_PRINT("📥 Received from Queue:");
            DEBUG_PRINTLN(rxMsg.data);
            DEBUG_PRINTLN("============================================");
            DEBUG_PRINTLN();
            // DEBUG_PRINTLN("\n📥 " + msg);

            int i1 = msg.indexOf(',');
            int i2 = msg.indexOf(',', i1 + 1);
            int i3 = msg.indexOf(',', i2 + 1);
            int i4 = msg.indexOf(',', i3 + 1);
            int i5 = msg.indexOf(',', i4 + 1);
            int i6 = msg.indexOf(',', i5 + 1);

            String sender    = msg.substring(0, i1);
            String receiver  = msg.substring(i1 + 1, i2);
            String command   = msg.substring(i2 + 1, i3);
            message_type_t type = (message_type_t) msg.substring(i3 + 1, i4).toInt();
            String msg_id    = msg.substring(i4 + 1, i5);
            String last_hop  = msg.substring(i5 + 1, i6);
            int hop_count    = msg.substring(i6 + 1).toInt();

            DEBUG_PRINTLN("Raw type: " + String(type));
            DEBUG_PRINTLN("Type: " + String(getTypeName(type)));

            //Reset acknowledgement flag
            needAck = false;

            // check for duplicates first to prevent loops and unnecessary processing
            if (isDuplicate(sender, type, msg_id)) {
                DEBUG_PRINTLN("⚠️ Duplicate ignored");
                continue;
            }

            // Rebroadcast If Needed
            rebroadcastIfNeeded(sender, receiver, command, type, msg_id, last_hop, hop_count);

            // Ignore Messages Not for Me
            if (receiver != nodeID && receiver != MasterID){
                DEBUG_PRINTLN("Not for me so ignoring... (receiver: " + receiver + ")");
                continue;
            }
            
            // Ignore ACK execution (Just extra saafety to prevent loops in case rebroadcast logic messed up)
            if (type == MSG_ACK) {
                DEBUG_PRINTLN("ACK received");
                continue;
            }

            DEBUG_PRINTLN("✅ CMD: " + command);

            // ================= LED For Debugging =================
            if (command == "red") {
                sendLedCommand(LED_RED);
                needAck = true;
            } 
            else if (command == "green") {
                needAck = true;
                sendLedCommand(LED_GREEN);
            }
            else if (command == "blue") {
                needAck = true;
                sendLedCommand(LED_BLUE);
            }
            else if (command == "off") {
                needAck = true;
                sendLedCommand(LED_IDLE);
            }
            //=========================================================

            //=============Set up ACK fields and send back================
            if(command == "ping" || command == "hb") {
                publisshHeartBeat();
                needAck = true;
            }

            if(command == "sd"){
                needAck = true;
                publishSensorData();
            }
            
            //Repeater on/off
            if(command == "repeater:1") {
                isRepeater = true;
                preferences.begin("device_config", false);
                preferences.putBool("is_repeater", true);
                preferences.end();
                needAck = true;
                sendLedCommand(LED_REPEATER_ON);
            }
            if(command == "repeater:0") {
                isRepeater = false;
                preferences.begin("device_config", false);
                preferences.putBool("is_repeater", false);
                preferences.end();
                needAck = true;
                sendLedCommand(LED_REPEATER_OFF);
            }

            //Set Maximum Forwards
            if(command.startsWith("max_fwds:")) {
                MAX_FWDS = command.substring(9).toInt();
                if(MAX_FWDS <= 20 || MAX_FWDS > 1000) {
                    MAX_FWDS = 50; // sanity check
                }
                preferences.begin("device_config", false);
                preferences.putInt("max_fwds", MAX_FWDS);
                preferences.end();
                needAck = true;
                sendLedCommand(LED_MAX_FWDS_SET);
            }

            //Set Maximum Hops
            if(command.startsWith("max_hops:")) {
                MAX_HOPS = command.substring(9).toInt();
                if(MAX_HOPS <= 1 || MAX_HOPS > 100) {
                    MAX_HOPS = 5; // sanity check
                }
                preferences.begin("device_config", false);
                preferences.putInt("max_hops", MAX_HOPS);
                preferences.end();
                needAck = true;
                sendLedCommand(LED_MAX_HOPS_SET);
            }

            //Set Heartbeat Interval
            if(command.startsWith("hb_interval:")) {
                hb_interval = command.substring(12).toInt();
                if(hb_interval <= 1 || hb_interval > 1440) {
                    hb_interval = 5; // sanity check
                }
                HB_INTERVAL = hb_interval * 60 * 1000;
                preferences.begin("device_config", false);
                preferences.putInt("hb_interval", hb_interval);
                preferences.end();
                needAck = true;
                sendLedCommand(LED_HEARTBEAT_SET);
            }

            //Set Encryption on/off
            if(command == "enc:1") {
                useEncryption = true;
                preferences.begin("device_config", false);
                preferences.putBool("use_encryption", true);
                preferences.end();

                needAck = true;
                sendLedCommand(LED_REPEATER_ON);
            }
            if(command == "enc:0") {
                useEncryption = false;
                preferences.begin("device_config", false);
                preferences.putBool("use_encryption", false);
                preferences.end();

                needAck = true;
                sendLedCommand(LED_REPEATER_OFF);
            }

            //Set Light Dependent Node on/off
            if(command == "lds:1") {
                onOffByLDR = true;
                preferences.begin("device_config", false);
                preferences.putBool("lds", true);
                preferences.end();

                needAck = true;
                sendLedCommand(LED_REPEATER_ON);
                publishSensorData();
            }

            if(command == "lds:0") {
                onOffByLDR = false;
                preferences.begin("device_config", false);
                preferences.putBool("lds", false);
                preferences.end();

                needAck = true;
                sendLedCommand(LED_REPEATER_OFF);
                publishSensorData();
            }

            //Set Node Dependent Light High and Low Value
            if(command.startsWith("lds_high:")) {
                ldrHighValue = command.substring(9).toInt();
                if(ldrHighValue >= 100) {
                    ldrHighValue = 99; // sanity check
                }

                if(ldrHighValue <= ldrLowValue){
                    ldrHighValue = ldrLowValue - 1;
                }
                ldrHighValue = ldrHighValue * 40;

                needAck = true;
                preferences.begin("device_config", false);
                preferences.putInt("ldsHigh", ldrHighValue);
                preferences.end();
                sendLedCommand(LED_MAX_FWDS_SET);
            }

            if(command.startsWith("lds_low:")) {
                ldrLowValue = command.substring(8).toInt();
                if(ldrLowValue <= 0) {
                    ldrLowValue = 1; // sanity check
                }
                if(ldrLowValue >= ldrHighValue){
                    ldrLowValue = ldrHighValue + 1;
                }
                ldrLowValue = ldrLowValue * 40;

                needAck = true;
                preferences.begin("device_config", false);
                preferences.putInt("ldsLow", ldrLowValue);
                preferences.end();
                sendLedCommand(LED_MAX_FWDS_SET);
            }

            if(command == "ldr_rev:1"){
                IS_LDR_REVERSE = true;

                preferences.begin("device_config", false);
                preferences.putBool("ldrRev", true);
                preferences.end();

                needAck = true;
                sendLedCommand(LED_REPEATER_ON);
                publishSensorData();
            }

            if(command == "ldr_rev:0"){
                IS_LDR_REVERSE = false;

                preferences.begin("device_config", false);
                preferences.putBool("ldrRev", false);
                preferences.end();

                needAck = true;
                sendLedCommand(LED_REPEATER_OFF);
                publishSensorData();
            }

            //Set Node Dependent Light High and Low Value
            if(command.startsWith("ct_ratio:")) {
                CT_CALIB_FACTOR = command.substring(9).toFloat();
                
                preferences.begin("device_config", false);
                preferences.putFloat("ctRatio", CT_CALIB_FACTOR);
                preferences.end();
                needAck = true;
                sendLedCommand(LED_MAX_FWDS_SET);
                publishSensorData();
            }

            // Handle switch commands
            //============================================================//

            if(command == "sw1:1" || command == "sw1:0" ||
                command == "sw2:1" || command == "sw2:0" ||
                command == "sw3:1" || command == "sw3:0" ||
                command == "sw4:1" || command == "sw4:0" ||
                command == "sw1234:1" || command == "sw1234:0") {
                DEBUG_PRINTLN("Handling switch command: " + command);
                needAck = true;
                handleSwitches(command);
            }

            //=========== Local OTA Mode =============
            if(command == "local_ota")
            {
                otaMode = true;
                otaStartTime = millis();
                needAck = true;
                sendLedCommand(LED_GREEN);
                xTaskCreatePinnedToCore(LocalOtaTask,"LocalOTA",LocalOtaTask_STACK,NULL,LocalOtaTask_PRIORITY,&LocalOtaTaskHandle,1);
            }

            // ================= SEND ACK =================
            if(needAck == false) {
                continue;
            }

            delay(random(70, 171));

            String ack =
                String(nodeID) + "," +
                sender + "," +
                command + "," +
                String(MSG_ACK) + "," +
                msg_id + "," +
                String(nodeID) + "," +
                "0";

            if(useEncryption) {
                String encAck = encryptSimple(ack, enckey);
                esp_now_send(broadcastAddress, (uint8_t *)encAck.c_str(), encAck.length());
                DEBUG_PRINTLN("📤 ACK Sent: " + encAck);
                DEBUG_PRINTLN("📤 Original ACK: " + decryptSimple(encAck, enckey));
            }
            else {
                esp_now_send(broadcastAddress, (uint8_t *)ack.c_str(), ack.length());
                DEBUG_PRINTLN("📤 ACK Sent: " + ack);
            }
            // sendLedCommand(LED_PING_ACK);
            // vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
}
