#pragma once

#include <FastLED.h>
#include "config.h"

// LED configuration
#define LED_PIN 4
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];

enum LedCommandType {
    LED_IDLE,
    LED_RED,
    LED_GREEN,
    LED_BLUE,
    LED_HEARTBEAT,
    LED_PING_ACK,
    LED_SWITCH_SINGLE_ON,
    LED_SWITCH_SINGLE_OFF,
    LED_SWITCH_ALL_ON,
    LED_SWITCH_ALL_OFF,
    LED_REPEATER_ON,
    LED_REPEATER_OFF,
    LED_HEARTBEAT_SET,
    LED_MAX_FWDS_SET,
    LED_MAX_HOPS_SET
};

QueueHandle_t ledCommandQueue = NULL;
#define LED_CMD_QUEUE_SIZE 10

// TaskHandle_t ledTaskHandle = NULL;
// #define LED_TASK_PRIORITY 1
// #define LED_TASK_STACK 2 * 1024

struct LedCommand {
    LedCommandType type;
    uint8_t brightness;
};

void sendLedCommand(LedCommandType type);
void ledTask(void* parameter);

void FastLED_setup(){
    // Initialize LED hardware
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(50);
    leds[0] = CRGB::Black;
    FastLED.show();

    ledCommandQueue = xQueueCreate(LED_CMD_QUEUE_SIZE, sizeof(LedCommand));
    sendLedCommand(LED_SWITCH_ALL_OFF);
    xTaskCreatePinnedToCore(ledTask, "LedTask", LED_TASK_STACK, NULL, LED_TASK_PRIORITY, &ledTaskHandle, 1);
}

void sendLedCommand(LedCommandType type) {
    if (ledCommandQueue == NULL) return;
    
    LedCommand cmd;
    cmd.type = type;
    cmd.brightness = 100;
    xQueueSend(ledCommandQueue, &cmd, 0);
}

void ledTask(void* parameter) {
    unsigned long lastBlinkTime = 0;
    bool blinkState = false;
    LedCommand currentCommand;
    currentCommand.type = LED_IDLE;
    currentCommand.brightness = 100;
    
    DEBUG_PRINTLN("LED Task started");
    
    while (1) {
        // Check for new LED commands
        LedCommand newCommand;
        if (xQueueReceive(ledCommandQueue, &newCommand, 0) == pdTRUE) {
            currentCommand = newCommand;
            lastBlinkTime = millis();
            blinkState = false;
        }
        
        // Execute LED command
        switch (currentCommand.type) {
            case LED_IDLE:
                leds[0] = CRGB::Black;
                break;

            case LED_RED:
                leds[0] = CRGB::Red;
                break;
                
            case LED_GREEN:
                leds[0] = CRGB::Green;
                break;
                
            case LED_BLUE:
                leds[0] = CRGB::Blue;
                break;
                
            case LED_HEARTBEAT:
                // Double green blink
                leds[0] = CRGB::Blue;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Black;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Blue;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Black;
                FastLED.show();

                currentCommand.type = LED_IDLE; // Reset to idle after execution
                break;
                
            case LED_PING_ACK:
                // Double Green blink
                leds[0] = CRGB::Green;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Black;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Green;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Black;
                FastLED.show();

                currentCommand.type = LED_IDLE; // Reset to idle after execution
                break;
                
            case LED_SWITCH_SINGLE_ON:
                // Single Green blink
                leds[0] = CRGB::Green;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(500));
                leds[0] = CRGB::Black;
                FastLED.show();

                currentCommand.type = LED_IDLE; // Reset to idle after execution
                break;
                
            case LED_SWITCH_SINGLE_OFF:
                // Single Pink blink
                leds[0] = CRGB::Pink;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(500));
                leds[0] = CRGB::Black;
                FastLED.show();

                currentCommand.type = LED_IDLE; // Reset to idle after execution
                break;
                
            case LED_SWITCH_ALL_ON:
                leds[0] = CRGB::Green;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(1000));
                leds[0] = CRGB::Black;
                FastLED.show();

                currentCommand.type = LED_IDLE; // Reset to idle after execution
                break;
                
            case LED_SWITCH_ALL_OFF:
                leds[0] = CRGB::Pink;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(1000));
                leds[0] = CRGB::Black;
                FastLED.show();

                currentCommand.type = LED_IDLE; // Reset to idle after execution
                break;
                
            case LED_REPEATER_ON:
                // Blue-Red blink
                leds[0] = CRGB::Blue;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Black;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Red;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Black;
                FastLED.show();

                currentCommand.type = LED_IDLE; // Reset to idle after execution
                break;
                
            case LED_REPEATER_OFF:
                // Red-Blue blink
                leds[0] = CRGB::Red;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Black;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Blue;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Black;
                FastLED.show();

                currentCommand.type = LED_IDLE; // Reset to idle after execution
                break;
            
            case LED_HEARTBEAT_SET:
                // Green-Blue flash
                leds[0] = CRGB::Green;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Black;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Blue;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Black;
                FastLED.show();

                currentCommand.type = LED_IDLE; // Reset to idle after execution
                break;

            case LED_MAX_FWDS_SET:
                // Blue-Green flash
                leds[0] = CRGB::Blue;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Black;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Green;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Black;
                FastLED.show();

                currentCommand.type = LED_IDLE; // Reset to idle after execution
                break;
                
            case LED_MAX_HOPS_SET:
                // Yellow-Green flash
                leds[0] = CRGB::Yellow;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Black;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Green;
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(250));

                leds[0] = CRGB::Black;
                FastLED.show();

                currentCommand.type = LED_IDLE; // Reset to idle after execution
                break;
        }
        
        FastLED.show();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}



