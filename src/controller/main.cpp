#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "shared.h"

// מונה ההודעות: מתחיל מאפס, ועולה בכל שליחה
uint32_t seq = 0;


// ===== onSent: ESP-NOW קורא לה אחרי כל שליחה =====

void onSent(const wifi_tx_info_t *mac, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("  -> Delivered");
  } else {
    Serial.println("  -> Delivery FAILED");
  }
}


// ===== שלב ההתחלה =====

void setup() {
  Serial.begin(115200);

  // 1. מדליקים את הרדיו במצב תחנה
  WiFi.mode(WIFI_STA);

  // 2. מפעילים את ESP-NOW, ובודקים שהצליח
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init FAILED");
    return;
  }

  // 3. רושמים את onSent, כדי לדעת אם ההודעות נמסרו
  esp_now_register_send_cb(onSent);

  // 4. רושמים את בקר המסך כשותף
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


// ===== הלולאה: שולחת הודעה כל שנייה =====

void loop() {

  // ממלאים כרטיס הודעה חדש
  Message msg;
  msg.seq = seq;
  strcpy(msg.text, "hello");

  // שולחים את כל הכרטיס לבקר המסך
  esp_now_send(DISPLAY_MAC, (const uint8_t *)&msg, sizeof(msg));

  Serial.printf("Sent #%lu\n", (unsigned long)msg.seq);

  // מגדילים את המונה להודעה הבאה
  seq++;

  delay(1000);
}