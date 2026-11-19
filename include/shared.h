#pragma once
#include <Arduino.h>

// ===== כתובות ה-MAC של שני הבקרים =====

// בקר א': השלט
static const uint8_t CONTROLLER_MAC[6] = {0x84, 0xFC, 0xE6, 0x57, 0xF8, 0xF8};

// בקר ב': המסך
static const uint8_t DISPLAY_MAC[6] = {0x94, 0xA9, 0x90, 0x30, 0x3B, 0xCC};


// ===== מבנה ההודעה: שני הבקרים משתמשים בו =====

struct Message {
  // מספר רצף: עולה באחד בכל הודעה
  uint32_t seq;

  // הטקסט, עד 31 תווים ועוד סימן סוף
  char text[32];
};
// ===== סוגי ההודעות של המשחק =====

 enum MsgType : uint8_t {
  Msg_Im_here=1,
  Msg_Push= 2,
 };
    

// ===== מבנה הודעת המשחק =====

struct GameMsg {
  uint8_t type;  
  uint32_t seq; 
};