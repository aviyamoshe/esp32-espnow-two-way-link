#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "shared.h"

#define HEARTBEAT_MS 1000
#define BUTTON_PIN   0
#define DEBOUNCE_MS  30

uint32_t seq = 0;
uint32_t lastHeartbeatTime = 0;

// Button debounce (active low, INPUT_PULLUP)
int lastReading = HIGH;       // raw reading from the previous loop
uint32_t lastChangeTime = 0;  // when the raw reading last changed
int stableState = HIGH;       // debounced state

volatile uint32_t sendFails = 0;
uint32_t printedFails = 0;

void onSent(const wifi_tx_info_t *mac, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) sendFails++;
}

void sendMsg(uint8_t type) {
  GameMsg msg = {};
  msg.type = type;
  msg.seq = seq++;
  esp_now_send(DISPLAY_MAC, (const uint8_t *)&msg, sizeof(msg));
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init FAILED");
    return;
  }
  esp_now_register_send_cb(onSent);

  // Register the display board as a peer
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, DISPLAY_MAC, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("Add peer FAILED");
    return;
  }

  Serial.println("Controller ready");
}

void loop() {
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastReading) lastChangeTime = millis();

  if (millis() - lastChangeTime >= DEBOUNCE_MS && reading != stableState) {
    stableState = reading;
    if (stableState == LOW) {
      Serial.println("PRESSED");
      sendMsg(Msg_Push);
    }
  }
  lastReading = reading;

  if (millis() - lastHeartbeatTime > HEARTBEAT_MS) {
    sendMsg(Msg_Im_here);
    lastHeartbeatTime = millis();

    if (sendFails != printedFails) {
      printedFails = sendFails;
      Serial.printf("Send failures so far: %lu\n", (unsigned long)printedFails);
    }
  }
}
