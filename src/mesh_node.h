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
struct MsgKey {
    char sender[10];
    uint8_t type;
    char msg_id[6];
};

std::deque<MsgKey> recentMsgKeys;
const size_t maxRecentIDs = 200; // Can be made configurable via Preferences

//================= ENCRYPTION =================
bool useEncryption = false;
const char enckey[] = "dmabd987";
const char encCharset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()_+=[]{}|:;<>?,./~";

// ================= MESSAGE ENUM =================
typedef enum {
    MSG_CMD = 1,
    MSG_ACK,
    MSG_HB,
    MSG_SD
} message_type_t;

//================= MESSAGE STRUCTURE =================
struct Message {
    char sender_id[10];
    char receiver_id[10];
    char command[40];
    uint8_t type;
    char msg_id[6];
    char last_hop[10];
    uint8_t hop_count;
};

// ================= ESP-NOW RECEIVE STRUCTURE =================
struct EspNowRxMessage {
    int len;
    char data[ESPNOW_MAX_MSG_LEN + 1];
};

// ================= QUEUE AND TASK HANDLES =================
TaskHandle_t EspNowOnReceiveTaskHandle = NULL;
QueueHandle_t espNowRxQueue = NULL;
#define ONRECEIVE_TASK_STACK 16 * 1024
#define ONRECEIVE_TASK_PRIORITY 2

//============= Function prototypes ================
const char* getTypeName(message_type_t type);
String generateMessageID();
void encryptSimple(const char* msg, char* out, const char* enckey);
void decryptSimple(const char* msg, char* out, const char* enckey);
bool isDuplicate(const char* sender, message_type_t type, const char* msg_id);
void rebroadcastIfNeeded(
    const char* sender,
    const char* receiver,
    const char* command,
    message_type_t type,
    const char* msg_id,
    const char* last_hop,
    int hop_count);
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
void encryptSimple(const char* msg, char* out, const char* enckey) {
    int msgLen = strlen(msg);
    int keyLen = strlen(enckey);
    int charsetLen = strlen(encCharset);

    for (int i = 0; i < msgLen; i++)
    {
        char c = msg[i];
        int index = -1;

        // find char in charset
        for (int j = 0; j < charsetLen; j++)
        {
            if (encCharset[j] == c)
            {
                index = j;
                break;
            }
        }

        if (index == -1)
        {
            out[i] = c;   // keep delimiters
            continue;
        }

        int shift = enckey[i % keyLen] + i;
        int newIndex = (index + shift) % charsetLen;

        out[i] = encCharset[newIndex];
    }

    out[msgLen] = '\0';
}

//================= DECRYPTION FUNCTIONS =================
void decryptSimple(const char* msg, char* out, const char* enckey) {
    int msgLen = strlen(msg);
    int keyLen = strlen(enckey);
    int charsetLen = strlen(encCharset);

    for (int i = 0; i < msgLen; i++)
    {
        char c = msg[i];
        int index = -1;

        for (int j = 0; j < charsetLen; j++)
        {
            if (encCharset[j] == c)
            {
                index = j;
                break;
            }
        }

        if (index == -1)
        {
            out[i] = c;
            continue;
        }

        int shift = enckey[i % keyLen] + i;
        int newIndex = index - shift;

        while (newIndex < 0)
            newIndex += charsetLen;

        out[i] = encCharset[newIndex];
    }

    out[msgLen] = '\0';
}

// ================= DEDUP =================
bool isDuplicate(const char* sender, message_type_t type, const char* msg_id) {
    MsgKey key;

    strncpy(key.sender, sender, sizeof(key.sender));
    key.sender[sizeof(key.sender) - 1] = '\0';

    key.type = type;

    strncpy(key.msg_id, msg_id, sizeof(key.msg_id));
    key.msg_id[sizeof(key.msg_id) - 1] = '\0';

    // search in deque
    for (auto &k : recentMsgKeys)
    {
        if (
            strcmp(k.sender, key.sender) == 0 &&
            k.type == key.type &&
            strcmp(k.msg_id, key.msg_id) == 0
        )
        {
            DEBUG_PRINTLN("Duplicate: " + String(k.sender) +":"+ String(k.type) +":"+ String(k.msg_id));
            return true;
        }
    }

    // insert new
    recentMsgKeys.push_back(key);

    if (recentMsgKeys.size() > maxRecentIDs)
    {
        recentMsgKeys.pop_front();
    }

    return false;
}

// ================= REBROADCAST =================
void rebroadcastIfNeeded(
    const char* sender,
    const char* receiver,
    const char* command,
    message_type_t type,
    const char* msg_id,
    const char* last_hop,
    int hop_count) {

    if (!isRepeater) return;

    if (strcmp(sender, nodeID) == 0) return;

    if (strcmp(last_hop, nodeID) == 0) return;

    if (strcmp(receiver, nodeID) == 0) return;

    //No rebroadcast if then maximum hops have been reached
    if (hop_count >= MAX_HOPS) {
        DEBUG_PRINTLN("⛔ Max hops reached");
        return;
    }

    delay(random(20, 70));

    // Update routing fields
    char newMsg[ESPNOW_MAX_MSG_LEN + 1];

    snprintf(
        newMsg,
        sizeof(newMsg),
        "%s,%s,%s,%d,%s,%s,%d",
        sender,
        receiver,
        command,
        (int)type,
        msg_id,
        nodeID,   // new_last_hop = nodeID
        hop_count + 1
    );

    char encNewMsg[ESPNOW_MAX_MSG_LEN + 1];

    if (useEncryption)
    {
        char encNewMsg[ESPNOW_MAX_MSG_LEN + 1];
        encryptSimple(newMsg, encNewMsg, enckey);
        DEBUG_PRINTLN("Rebrodcasted: " +String(encNewMsg));

        esp_now_send(
            broadcastAddress,
            (uint8_t *)encNewMsg,
            strlen(encNewMsg)
        );
    }
    else
    {
        DEBUG_PRINTLN("Rebrodcasted: " +String(newMsg));
        esp_now_send(
            broadcastAddress,
            (uint8_t *)newMsg,
            strlen(newMsg)
        );
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
            // String msg = String(rxMsg.data);
            char msg[ESPNOW_MAX_MSG_LEN + 1];

            int len = rxMsg.len;
            if (len > ESPNOW_MAX_MSG_LEN) len = ESPNOW_MAX_MSG_LEN;

            memcpy(msg, rxMsg.data, len);
            msg[len] = '\0';

            // Expect 7 fields (6 commas)
            // int commas = std::count(msg.begin(), msg.end(), ',');
            int commas = 0;

            for(int i = 0; msg[i] != '\0'; i++)
            {
                if(msg[i] == ',')
                    commas++;
            }

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

            char *saveptr;

            char *sender =
                strtok_r(msg, ",", &saveptr);

            char *receiver =
                strtok_r(NULL, ",", &saveptr);

            char *command =
                strtok_r(NULL, ",", &saveptr);

            char *typeStr =
                strtok_r(NULL, ",", &saveptr);

            char *msg_id =
                strtok_r(NULL, ",", &saveptr);

            char *last_hop =
                strtok_r(NULL, ",", &saveptr);

            char *hopStr =
                strtok_r(NULL, ",", &saveptr);

            if(
                sender == NULL ||
                receiver == NULL ||
                command == NULL ||
                typeStr == NULL ||
                msg_id == NULL ||
                last_hop == NULL ||
                hopStr == NULL
            )
            {
                DEBUG_PRINTLN("❌ Invalid packet");
                continue;
            }

            message_type_t type =
                (message_type_t)atoi(typeStr);

            int hop_count = atoi(hopStr);

            sender[strcspn(sender, "\r\n\t ")] = 0;
            receiver[strcspn(receiver, "\r\n\t ")] = 0;
            command[strcspn(command, "\r\n\t ")] = 0;
            msg_id[strcspn(msg_id, "\r\n\t ")] = 0;
            last_hop[strcspn(last_hop, "\r\n\t ")] = 0;
            typeStr[strcspn(typeStr, "\r\n\t ")] = 0;
            hopStr[strcspn(hopStr, "\r\n\t ")] = 0;

            DEBUG_PRINT("Raw type: " + String(type));
            DEBUG_PRINTLN(" | Type: " + String(getTypeName(type)));
            DEBUG_PRINT("CMD FINAL:");
            DEBUG_PRINTLN(command);

            //Reset acknowledgement flag
            needAck = false;

            // check duplicates
            if (isDuplicate(sender, type, msg_id)) {
                DEBUG_PRINTLN("⚠️ Duplicate ignored");
                continue;
            }

            // FIRST: check if packet is for me
            if (strcmp(receiver, nodeID) != 0 && strcmp(receiver, MasterID) != 0) {
                // still allow rebroadcast BEFORE skipping
                rebroadcastIfNeeded(sender, receiver, command, type, msg_id, last_hop, hop_count);
                continue;
            }

            // Rebroadcast AFTER validation (optional safer version)
            rebroadcastIfNeeded(sender, receiver, command, type, msg_id, last_hop, hop_count);
            
            // Ignore ACK execution (Just extra saafety to prevent loops in case rebroadcast logic messed up)
            if (type == MSG_ACK) {
                DEBUG_PRINTLN("ACK received");
                continue;
            }

            // DEBUG_PRINTLN("✅ CMD: " + command);

            // ================= LED For Debugging =================
            if (strcmp(command, "red") == 0) {
                sendLedCommand(LED_RED);
                needAck = true;
            }
            else if (strcmp(command, "green") == 0) {
                sendLedCommand(LED_GREEN);
                needAck = true;
            }
            else if (strcmp(command, "blue") == 0) {
                sendLedCommand(LED_BLUE);
                needAck = true;
            }
            else if (strcmp(command, "off") == 0) {
                sendLedCommand(LED_IDLE);
                needAck = true;
            }
            //=========================================================

            //=============Set up ACK fields and send back================
            if (strcmp(command, "ping") == 0 || strcmp(command, "hb") == 0) {
                publisshHeartBeat();
                needAck = true;
            }

            if (strcmp(command, "sd") == 0){
                needAck = true;
                // publishSensorData();
            }
            
            //Repeater on/off
            if (strcmp(command, "repeater:1") == 0) {
                isRepeater = true;
                preferences.begin("device_config", false);
                preferences.putBool("is_repeater", true);
                preferences.end();
                needAck = true;
                sendLedCommand(LED_REPEATER_ON);
            }
            if (strcmp(command, "repeater:0") == 0) {
                isRepeater = false;
                preferences.begin("device_config", false);
                preferences.putBool("is_repeater", false);
                preferences.end();
                needAck = true;
                sendLedCommand(LED_REPEATER_OFF);
            }

            //Set Maximum Forwards
            if (strncmp(command, "max_fwds:", 9) == 0) {
                int val = atoi(command + 9);

                MAX_FWDS = val;

                if (MAX_FWDS <= 20 || MAX_FWDS > 1000)
                {
                    MAX_FWDS = 50; // sanity check
                }

                preferences.begin("device_config", false);
                preferences.putInt("max_fwds", MAX_FWDS);
                preferences.end();

                needAck = true;
                sendLedCommand(LED_MAX_FWDS_SET);
            }

            //Set Maximum Hops
            if (strncmp(command, "max_hops:", 9) == 0) {
                int val = atoi(command + 9);

                MAX_HOPS = val;

                if (MAX_HOPS <= 1 || MAX_HOPS > 100)
                {
                    MAX_HOPS = 5; // sanity check
                }

                preferences.begin("device_config", false);
                preferences.putInt("max_hops", MAX_HOPS);
                preferences.end();

                needAck = true;
                sendLedCommand(LED_MAX_HOPS_SET);
            }

            //Set Heartbeat Interval
            if (strncmp(command, "hb_interval:", 12) == 0) {
                int val = atoi(command + 12);

                hb_interval = val;

                if (hb_interval <= 1 || hb_interval > 1440)
                {
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
            if (strcmp(command, "enc:1") == 0) {
                useEncryption = true;
                preferences.begin("device_config", false);
                preferences.putBool("use_encryption", true);
                preferences.end();

                needAck = true;
                sendLedCommand(LED_REPEATER_ON);
            }

            if (strcmp(command, "enc:0") == 0) {
                useEncryption = false;
                preferences.begin("device_config", false);
                preferences.putBool("use_encryption", false);
                preferences.end();

                needAck = true;
                sendLedCommand(LED_REPEATER_OFF);
            }

            //=========== Local OTA Mode =============
            if (strcmp(command, "local_ota") == 0) {
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

            char ack[ESPNOW_MAX_MSG_LEN + 1];

            snprintf(
                ack,
                sizeof(ack),
                "%s,%s,%s,%d,%s,%s,%d",
                nodeID,
                sender,
                command,
                MSG_ACK,
                msg_id,
                nodeID,
                0
            );

            if(useEncryption) {

                char encAck[ESPNOW_MAX_MSG_LEN + 1];

                encryptSimple(ack, encAck, enckey);

                esp_now_send(
                    broadcastAddress,
                    (uint8_t*)encAck,
                    strlen(encAck)
                );

            } else {

                esp_now_send(
                    broadcastAddress,
                    (uint8_t*)ack,
                    strlen(ack)
                );

                DEBUG_PRINT("Sent Ack: ");
                DEBUG_PRINTLN(ack);
            }
            // sendLedCommand(LED_PING_ACK);
            // vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
}
