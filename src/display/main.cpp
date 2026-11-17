#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "shared.h"


// ===== הרגליים של המסך =====

#define LCD_MOSI 45
#define LCD_SCLK 40
#define LCD_CS   42
#define LCD_DC   41
#define LCD_RST  39
#define LCD_BL   48

// כמה זמן של שקט נחשב לניתוק, באלפיות שנייה
#define LINK_TIMEOUT_MS 3000


// ===== סוגי האירועים שמשימת המסך מקבלת =====

// הגיעה הודעה מבקר השלט
#define EVT_MESSAGE    1

// הקשר נותק
#define EVT_LINK_LOST  2

// הקשר תקין
#define EVT_LINK_OK    3


// ===== הקופסה שנכנסת לתור של המסך =====

struct DisplayEvent {
  // סוג האירוע: אחד משלושת המספרים למעלה
  uint8_t type;

  // ההודעה עצמה. בשימוש רק כשהסוג הוא EVT_MESSAGE
  Message msg;
};


// ===== משתנים גלובליים =====

// המסך
Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, LCD_CS, LCD_DC, LCD_RST);

// התור של המסך
QueueHandle_t displayQueue;

// מתי הגיעה ההודעה האחרונה
volatile uint32_t lastRxTime = 0;

// האם הגיעה בכלל הודעה מאז ההדלקה
volatile bool everReceived = false;


// ===== onReceive: ESP-NOW קורא לה כשמגיעה הודעה =====

void onReceive(const esp_now_recv_info*mac, const uint8_t *data, int len) {

  // בדיקת בטיחות: אם הגודל לא מתאים להודעה שלנו, מתעלמים
  if (len != sizeof(Message)) {
    return;
  }

  // ממלאים קופסה
  DisplayEvent evt;
  evt.type = EVT_MESSAGE;
  memcpy(&evt.msg, data, sizeof(Message));

  // ליתר ביטחון: מוודאים שיש סימן סוף בטקסט
  evt.msg.text[31] = '\0';

  // רושמים מתי הגיעה ההודעה
  lastRxTime = millis();
  everReceived = true;

  // שמים בתור, ויוצאים מיד
  xQueueSend(displayQueue, &evt, 0);
}


// ===== פונקציות ציור: רק משימת המסך קוראת להן =====

// מציירת את נקודת מצב הקשר בפינה הימנית העליונה
void drawLinkStatus(bool ok) {

  // מוחקים את האזור הקודם
  tft.fillRect(95, 5, 77, 25, ST77XX_BLACK);

  // בוחרים צבע לפי המצב
  uint16_t color = ok ? ST77XX_GREEN : ST77XX_RED;

  // הנקודה
  tft.fillCircle(160, 17, 8, color);

  // המילה ליד הנקודה
  tft.setTextSize(1);
  tft.setTextColor(color);
  tft.setCursor(115, 14);
  tft.print(ok ? "LINK" : "LOST");
}

// מציירת הודעה שהתקבלה, באזור האמצעי של המסך
void drawMessage(const Message &msg) {

  // מוחקים את אזור ההודעה הקודמת
  tft.fillRect(0, 50, 172, 150, ST77XX_BLACK);

  // כותרת קטנה
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(10, 60);
  tft.print("Received:");

  // הטקסט עצמו, גדול ובצהוב
  tft.setTextSize(3);
  tft.setTextColor(ST77XX_YELLOW);
  tft.setCursor(10, 100);
  tft.print(msg.text);

  // מספר הרצף, בתכלת
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(10, 150);
  tft.printf("seq: %lu", (unsigned long)msg.seq);
}


// ===== משימת המסך: עדיפות 2 =====

void displayTask(void *p) {

  DisplayEvent evt;

  while (true) {

    // ישנה עד שמגיעה קופסה
    xQueueReceive(displayQueue, &evt, portMAX_DELAY);

    // מחליטה מה לצייר לפי סוג האירוע
    if (evt.type == EVT_MESSAGE) {
      drawMessage(evt.msg);
      Serial.printf("Got #%lu: %s\n", (unsigned long)evt.msg.seq, evt.msg.text);
    }
    else if (evt.type == EVT_LINK_OK) {
      drawLinkStatus(true);
      Serial.println("Link OK");
    }
    else if (evt.type == EVT_LINK_LOST) {
      drawLinkStatus(false);
      Serial.println("Link LOST");
    }
  }
}


// ===== משימת שומר הקשר: עדיפות 1 =====

void linkMonitorTask(void *p) {

  // בהתחלה אין קשר, עד שתגיע הודעה ראשונה
  bool linkOk = false;

  while (true) {

    // הקשר תקין אם הגיעה הודעה, והאחרונה הגיעה לפני פחות מ-3 שניות
    bool alive = everReceived && (millis() - lastRxTime <= LINK_TIMEOUT_MS);

    // הקשר חזר: מודיעים למסך פעם אחת
    if (alive && !linkOk) {
      DisplayEvent evt;
      evt.type = EVT_LINK_OK;
      xQueueSend(displayQueue, &evt, 0);
      linkOk = true;
    }

    // הקשר נותק: מודיעים למסך פעם אחת
    if (!alive && linkOk) {
      DisplayEvent evt;
      evt.type = EVT_LINK_LOST;
      xQueueSend(displayQueue, &evt, 0);
      linkOk = false;
    }

    // ישנה חצי שנייה
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}


// ===== שלב ההתחלה =====

void setup() {
  Serial.begin(115200);

  // --- המסך ---

  // מדליקים את התאורה האחורית
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  // מפעילים את התקשורת עם המסך, ומאתחלים אותו
  SPI.begin(LCD_SCLK, -1, LCD_MOSI, LCD_CS);
  tft.init(172, 320);
  tft.fillScreen(ST77XX_BLACK);

  // כותרת קבועה למעלה
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(5, 10);
  tft.print("ESP-NOW");

  // בהתחלה: אין קשר
  drawLinkStatus(false);

  // --- התור: חייב להיווצר לפני ש-onReceive נרשמת ---
  displayQueue = xQueueCreate(10, sizeof(DisplayEvent));

  // --- ESP-NOW ---

  // מדליקים את הרדיו
  WiFi.mode(WIFI_STA);

  // מפעילים את ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init FAILED");
    return;
  }

  // רושמים את onReceive
  esp_now_register_recv_cb(onReceive);

  // --- המשימות ---
  xTaskCreate(displayTask,     "Display", 8192, NULL, 2, NULL);
  xTaskCreate(linkMonitorTask, "LinkMon", 4096, NULL, 1, NULL);

  Serial.println("Display ready");
}


// ===== הלולאה הראשית: לא עושה כלום =====

void loop() {
  vTaskDelay(portMAX_DELAY);
}