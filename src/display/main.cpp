#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "shared.h"

#define LCD_MOSI 45
#define LCD_SCLK 40
#define LCD_CS   42
#define LCD_DC   41
#define LCD_RST  39
#define LCD_BL   48

#define LINK_TIMEOUT_MS 3000

#define EVT_PRESS     1
#define EVT_LINK_LOST 2
#define EVT_LINK_OK   3

struct DisplayEvent {
  uint8_t type;
  GameMsg msg;
  uint32_t rxTime;
};

Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, LCD_CS, LCD_DC, LCD_RST);
QueueHandle_t displayQueue;
volatile uint32_t lastRxTime = 0;
volatile bool everReceived = false;

enum GameState : uint8_t { //מצבי המשחק תכנון MOORE// המצב אף פעם לא יוצא מהבקר הזה לכן אין צורך במספרים מפורשים כמו שעשיתי בMSG_TYPE
  STATE_WAITING,   // waiting for the player to start
  STATE_GET_READY, // random timing until the player need to push the buttom
  STATE_NOW,       // green screen, press now
  STATE_TOO_EARLY, // pressed before green,comeback to the start
  STATE_RESULT     // show reaction time
};
GameState state = STATE_WAITING;
uint32_t lastReactionMs = 0;
uint32_t bestReactionMs = 0;

void onReceive(const esp_now_recv_info *mac, const uint8_t *data, int len) {
  if (len != sizeof(GameMsg)) return;

  GameMsg msg;
  memcpy(&msg, data, sizeof(GameMsg));
  lastRxTime = millis();
  everReceived = true;

  if (msg.type == Msg_Push) { // בדיקה של סוג ההודעה
    DisplayEvent evt;
    evt.type = EVT_PRESS;
    evt.msg = msg;
    evt.rxTime = millis();
    xQueueSend(displayQueue, &evt, 0);
  }
}

void drawLinkStatus(bool ok) {
  uint16_t color = ok ? ST77XX_GREEN : ST77XX_RED;
  tft.fillRect(95, 5, 77, 25, ST77XX_BLACK);
  tft.fillCircle(160, 17, 8, color);
  tft.setTextSize(1);
  tft.setTextColor(color);
  tft.setCursor(115, 14);
  tft.print(ok ? "LINK" : "LOST");
}

#define GAME_TOP 40
#define SCREEN_W 172
#define SCREEN_H 320

void drawCentered(const char *text, int y, uint8_t size, uint16_t color) {
  int16_t x1, y1;
  uint16_t w, h;
  tft.setTextSize(size);
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  tft.setTextColor(color);
  tft.setCursor((SCREEN_W - w) / 2, y);
  tft.print(text);
}

void drawState(GameState s) {
  char line[24];
  switch (s) {
    case STATE_WAITING:
      tft.fillRect(0, GAME_TOP, SCREEN_W, SCREEN_H - GAME_TOP, ST77XX_BLACK);
      drawCentered("REACTION", 110, 3, ST77XX_CYAN);
      drawCentered("GAME", 145, 3, ST77XX_CYAN);
      drawCentered("PRESS TO", 220, 2, ST77XX_WHITE);
      drawCentered("START", 245, 2, ST77XX_WHITE);
      break;

    case STATE_GET_READY:
      tft.fillRect(0, GAME_TOP, SCREEN_W, SCREEN_H - GAME_TOP, ST77XX_RED);
      drawCentered("GET", 150, 3, ST77XX_WHITE);
      drawCentered("READY", 185, 3, ST77XX_WHITE);
      break;

    case STATE_NOW:
      tft.fillRect(0, GAME_TOP, SCREEN_W, SCREEN_H - GAME_TOP, ST77XX_GREEN);
      drawCentered("NOW!", 160, 4, ST77XX_BLACK);
      break;

    case STATE_TOO_EARLY:
      tft.fillRect(0, GAME_TOP, SCREEN_W, SCREEN_H - GAME_TOP, ST77XX_ORANGE);
      drawCentered("TOO", 140, 3, ST77XX_BLACK);
      drawCentered("EARLY!", 175, 3, ST77XX_BLACK);
      drawCentered("PRESS TO RETRY", 250, 1, ST77XX_BLACK);
      break;

    case STATE_RESULT:
      tft.fillRect(0, GAME_TOP, SCREEN_W, SCREEN_H - GAME_TOP, ST77XX_BLACK);
      drawCentered("YOUR TIME", 90, 2, ST77XX_WHITE);
      snprintf(line, sizeof(line), "%lu ms", (unsigned long)lastReactionMs);
      drawCentered(line, 125, 3, ST77XX_YELLOW);
      if (bestReactionMs > 0) {
        snprintf(line, sizeof(line), "BEST: %lu ms", (unsigned long)bestReactionMs);
        drawCentered(line, 190, 2, ST77XX_CYAN);
      }
      drawCentered("PRESS TO PLAY AGAIN", 260, 1, ST77XX_WHITE);
      break;
  }
}

void setState(GameState newState) { // קובעת את המצב החדש בSTATE בכל פעם
  state = newState;
  drawState(state);
  Serial.printf("State -> %d\n", (int)state);
}

#define MIN_WAIT_MS    2000
#define MAX_WAIT_MS    5000
#define NOW_TIMEOUT_MS 3000

uint32_t deadlineMs = 0;
uint32_t greenAtMs = 0;

void startRound() {
  deadlineMs = millis() + random(MIN_WAIT_MS, MAX_WAIT_MS); // זמן המעבר למסך הירוק והמסך האדום יהיה מרגע מעבר למסך האדום פלוס זמן רנדומלי בין הגבולות שהגדרתי
  setState(STATE_GET_READY);
}

void displayTask(void *p) {
  DisplayEvent evt;
  drawState(state);

  while (true) {
    TickType_t wait = portMAX_DELAY;
    if (state == STATE_GET_READY || state == STATE_NOW) {
      int32_t left = (int32_t)(deadlineMs - millis());
      wait = (left > 0) ? pdMS_TO_TICKS(left) : 0;
    }

    bool got = (xQueueReceive(displayQueue, &evt, wait) == pdTRUE);
    if (got && evt.type == EVT_LINK_OK)   { drawLinkStatus(true);  continue; }
    if (got && evt.type == EVT_LINK_LOST) { drawLinkStatus(false); continue; }

    bool pressed = got && evt.type == EVT_PRESS;
    bool timeUp = !got;

    switch (state) {
      case STATE_WAITING:
        if (pressed) startRound();
        break;

      case STATE_GET_READY:
        if (pressed) { // אם יש לחיצה ואנחנו במצב של המסך האדום הלחיצה קרתה מוקדם מדי
          setState(STATE_TOO_EARLY);
        }
        if (timeUp) {
          setState(STATE_NOW);
          greenAtMs = millis();
          deadlineMs = millis() + NOW_TIMEOUT_MS; // כשהמסך עובר לירוק יש זמן של 3 שניות לשחקנית ללחוץ לכן בכל פעם שעובר לירוק נגדיר את הזמן ועוד 3 שניות  שהגדרנו לחכות לזמן לחיצה כשהמסך ירוק
        }
        break;

      case STATE_NOW:
        if (pressed) {
          lastReactionMs = evt.rxTime - greenAtMs; // זמן הקבלה לתור של פונקצית ONRECIEVE מוגדר  ולכן זמן התגובה יהיה הזמן המוגדר ברגע שהתקבלה ההודעה ונכנסה לתור פחות הזמן בו המסך עבר לירוק
          if (lastReactionMs < bestReactionMs || bestReactionMs == 0) {
            bestReactionMs = lastReactionMs;
          }
          setState(STATE_RESULT);
        }
        if (timeUp) startRound();
        break;

      case STATE_TOO_EARLY:
        if (pressed) startRound();
        break;

      case STATE_RESULT:
        if (pressed) setState(STATE_WAITING);
        break;
    }
  }
}

void linkMonitorTask(void *p) {
  bool linkOk = false;

  while (true) {
    bool alive = everReceived && (millis() - lastRxTime <= LINK_TIMEOUT_MS);

    if (alive != linkOk) {
      linkOk = alive;
      DisplayEvent evt;
      evt.type = linkOk ? EVT_LINK_OK : EVT_LINK_LOST;
      xQueueSend(displayQueue, &evt, 0);
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  SPI.begin(LCD_SCLK, -1, LCD_MOSI, LCD_CS);
  tft.init(172, 320);
  tft.fillScreen(ST77XX_BLACK);

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(5, 10);
  tft.print("ESP-NOW");
  drawLinkStatus(false);

  displayQueue = xQueueCreate(10, sizeof(DisplayEvent));

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init FAILED");
    return;
  }
  esp_now_register_recv_cb(onReceive);

  xTaskCreate(displayTask,     "Display", 8192, NULL, 2, NULL);
  xTaskCreate(linkMonitorTask, "LinkMon", 4096, NULL, 1, NULL);

  Serial.println("Display ready");
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}
