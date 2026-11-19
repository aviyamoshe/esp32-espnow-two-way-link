#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "shared.h"

// מונה ההודעות: מתחיל מאפס, ועולה בכל שליחה
uint32_t seq = 0;
// ===== הכפתור =====

#define BUTTON_PIN  0     // כפתור BOOT
#define DEBOUNCE_MS 30    // כמה זמן הקריאה צריכה להיות יציבה

// TODO 1: מה הערך ההתחלתי של שלושת המשתנים?
//   רמז: כשמדליקים את הבקר, הכפתור משוחרר. ומה קוראים כשהוא משוחרר?
//   (בשעה, שימי פשוט 0)
int      lastReading    = HIGH;   // הקריאה בסיבוב הקודם
uint32_t lastChangeTime = 0;   // מתי הקריאה השתנתה לאחרונה
int      stableState    = HIGH;   // המצב היציב שהחלטנו עליו

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
  pinMode(BUTTON_PIN, INPUT_PULLUP);
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
// ===== הלולאה: בודקת את הכפתור כל הזמן =====

void loop() {

  // 1. קוראים את הכפתור עכשיו
  // TODO 3: איזו פונקציה קוראת את מצב הרגל?
  int reading =digitalRead (BUTTON_PIN) ;

  // 2. אם הקריאה השתנתה מהסיבוב הקודם, שומרים את השעה
  // TODO 4: מה התנאי? רמז: משווים בין שני משתנים
  if (reading != lastReading ) {
    // TODO 5: איזו פונקציה נותנת את השעה עכשיו?
    lastChangeTime = millis();
  }

  // 3. אם הקריאה יציבה כבר 30 אלפיות שנייה, והיא שונה מהמצב היציב
  if (millis() - lastChangeTime >= DEBOUNCE_MS && reading != stableState) {
    stableState = reading;

    // 4. אם המצב היציב החדש הוא "לחוץ", זו לחיצה!
    // TODO 6: מה התנאי? רמז: פעיל בנמוך
    if (stableState==LOW) {
      Serial.println("PRESSED");
    }
  }

  // 5. זוכרים את הקריאה, לסיבוב הבא
  lastReading = reading;
}



