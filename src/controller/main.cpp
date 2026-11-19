#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "shared.h"

uint32_t seq = 0;
// ===== הדופק =====

#define HEARTBEAT_MS 1000        
uint32_t lastHeartbeatTime = 0;  

#define BUTTON_PIN  0     // כפתור BOOT
#define DEBOUNCE_MS 30    // כמה זמן הקריאה צריכה להיות יציבה
int      lastReading    = HIGH;   // הקריאה בסיבוב הקודם
uint32_t lastChangeTime = 0;   // מתי הקריאה השתנתה לאחרונה
int      stableState    = HIGH;   // המצב היציב שהחלטנו עליו



// כמה שליחות נכשלו מאז ההדלקה. onSent כותבת, הלולאה קוראת ומדפיסה
volatile uint32_t sendFails = 0;
uint32_t printedFails = 0;   // כמה כישלונות כבר הדפסנו

// ===== onSent: ESP-NOW קורא לה אחרי כל שליחה =====
// רצה מתוך משימת הרדיו, ולכן חייבת להיות מהירה: רק סופרת, ולא מדפיסה

void onSent(const wifi_tx_info_t *mac, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS) {
    sendFails++;
  }
}
// ===== sendMsg: בונה הודעת משחק ושולחת לבקר המסך =====

void sendMsg(uint8_t type) {

  // טופס ריק, מאופס
  GameMsg msg = {};

  // TODO 1: מלאי את שתי המשבצות של הטופס
  //   רמז: את הסוג קיבלנו בפרמטר. ואת המונה כבר יש לנו, משתנה גלובלי
  msg.type = type;
  msg.seq  =seq ;

  // TODO 2: שולחים. כמה בתים לשלוח?
  //   רמז: תסתכלי איך עשו את זה בלולאה הישנה
  esp_now_send(DISPLAY_MAC, (const uint8_t *)&msg, sizeof(msg));

  // מגדילים את המונה להודעה הבאה
  seq++;
}

// ===== שלב ההתחלה =====

void setup() {
  Serial.begin(115200);

  // אם אף אחד לא קורא את ההדפסות במחשב, לא מחכים: פשוט מוותרים עליהן
  Serial.setTxTimeoutMs(0);

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


void loop() {

  // 1. קוראים את הכפתור עכשיו
  // TODO 3: איזו פונקציה קוראת את מצב הרגל?
  int reading =digitalRead (BUTTON_PIN) ;

  
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
  sendMsg(Msg_Push);
    }
  }
lastReading = reading;

  // 🆕 6. הדופק: אם עברה שנייה מאז "אני כאן" האחרון, שולחים שוב
  // TODO 4: מה התנאי?
  if (millis() - lastHeartbeatTime >HEARTBEAT_MS) {
    sendMsg(Msg_Im_here);
    // TODO 5: שלחי הודעת "אני כאן"
    
    // TODO 6: שמרי את השעה של השליחה
    lastHeartbeatTime = millis();

    // מדפיסים רק אם היו כישלונות חדשים מאז הפעם הקודמת
    if (sendFails != printedFails) {
      printedFails = sendFails;
      Serial.printf("Send failures so far: %lu\n", (unsigned long)printedFails);
    }
  }
}




