#pragma once

#include <config.h>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <deque>
#include <algorithm>
#include "led.h"

typedef enum {
    MSG_CMD = 1,
    MSG_ACK = 2,
    MSG_HEARTBEAT = 3,
    MSG_SENSOR = 4
} message_type_t;

// Debug helper
const char* getTypeName(message_type_t type) {
    switch(type) {
        case MSG_CMD: return "CMD";
        case MSG_ACK: return "ACK";
        case MSG_HEARTBEAT: return "HEARTBEAT";
        case MSG_SENSOR: return "SENSOR";
        default: return "UNKNOWN";
    }
}

uint8_t broadcastAddress[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

bool useEncryption = false;
String enckey = "dmabd987";
String encCharset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()_+=[]{}|:;<>?,./~";

String encryptSimple(String msg, String enckey)
{
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

String decryptSimple(String msg, String enckey)
{
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


// ================= CACHE =================
std::deque<String> fwdCache;
String lastCmdID;

bool alreadyForwarded(const String &key) {
    return std::find(fwdCache.begin(), fwdCache.end(), key) != fwdCache.end();
}

void recordForward(const String &key) {
    fwdCache.push_back(key);
    if (fwdCache.size() > MAX_FWDS)
    fwdCache.pop_front();
}

// ================= REBROADCAST =================
void rebroadcastIfNeeded(String sender, String receiver, String command,
    message_type_t type, String msg_id,
    String last_hop, int hop_count) {

    if (!isRepeater) return;

    String key = sender + ":" + msg_id;

    if (alreadyForwarded(key)) {
        DEBUG_PRINTLN("🔁 Already forwarded");
        return;
    }

    if (sender == nodeID) return;
    if (last_hop == nodeID) return;

    if (type == MSG_CMD && receiver == nodeID) return;

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
    recordForward(key);
}

// ================= RECEIVE =================
void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
    String rawMsg((char *)data, len);
    // DEBUG_PRINTLN("\n📥 " + rawMsg);
    String msg;
    if(useEncryption) {
        msg = decryptSimple(rawMsg, enckey);
    }
    else {
        msg = rawMsg;
    }

    // Expect 7 fields (6 commas)
    int commas = std::count(msg.begin(), msg.end(), ',');
    if (commas != 6) {
        // DEBUG_PRINTLN("❌ Invalid packet");
        return;
    }
    DEBUG_PRINTLN("\n📥 " + msg);

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

    DEBUG_PRINTLN("Type: " + String(getTypeName(type)));

    // Rebroadcast first
    rebroadcastIfNeeded(sender, receiver, command, type, msg_id, last_hop, hop_count);

    // Not for me
    if (receiver != nodeID) return;

    // Ignore ACK execution
    if (type == MSG_ACK) {
        DEBUG_PRINTLN("ACK received");
        return;
    }

    // Duplicate protection
    if (msg_id == lastCmdID) {
        DEBUG_PRINTLN("⚠️ Duplicate CMD");
        return;
    }
    lastCmdID = msg_id;

    DEBUG_PRINTLN("✅ CMD: " + command);

    // ================= LED For Debugging =================
    if (command == "red") leds[0] = CRGB::Red;
    else if (command == "green") leds[0] = CRGB::Green;
    else if (command == "blue") leds[0] = CRGB::Blue;
    else if (command == "off") leds[0] = CRGB::Black;

    FastLED.show();
    //=========================================================

    //=============Set up ACK fields and send back================
    if(command == "repeater:1") {
        isRepeater = true;
        preferences.begin("device_config", false);
        preferences.putBool("is_repeater", true);
        preferences.end();
    }
    if(command == "repeater:0") {
        isRepeater = false;
        preferences.begin("device_config", false);
        preferences.putBool("is_repeater", false);
        preferences.end();
    }

    if(command.startsWith("max_fwds:")) {
        MAX_FWDS = command.substring(9).toInt();
        if(MAX_FWDS <= 20 || MAX_FWDS > 500) {
            MAX_FWDS = 50; // sanity check
        }
        preferences.begin("device_config", false);
        preferences.putInt("max_fwds", MAX_FWDS);
        preferences.end();
    }
    if(command.startsWith("max_hops:")) {
        MAX_HOPS = command.substring(9).toInt();
        if(MAX_HOPS <= 1 || MAX_HOPS > 100) {
            MAX_HOPS = 5; // sanity check
        }
        preferences.begin("device_config", false);
        preferences.putInt("max_hops", MAX_HOPS);
        preferences.end();
    }
    if(command.startsWith("hb_interval:")) {
        hb_interval = command.substring(12).toInt();
        if(hb_interval <= 1 || hb_interval > 1440) {
            hb_interval = 5; // sanity check
        }
        HB_INTERVAL = hb_interval * 60 * 1000;
        preferences.begin("device_config", false);
        preferences.putInt("hb_interval", hb_interval);
        preferences.end();
    }

    // ================= SEND ACK =================
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
}

void mesh_node_setup() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init Failed");
        return;
    }

    esp_now_peer_info_t pi = {};
    memcpy(pi.peer_addr, broadcastAddress, 6);
    pi.channel = 0;
    pi.encrypt = false;
    esp_now_add_peer(&pi);

    esp_now_register_recv_cb(onReceive);
}
