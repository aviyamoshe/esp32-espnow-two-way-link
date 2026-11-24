#pragma once
#include <Arduino.h>

// MAC addresses of the two boards
static const uint8_t CONTROLLER_MAC[6] = {0x84, 0xFC, 0xE6, 0x57, 0xF8, 0xF8};  // board A: controller (button)
static const uint8_t DISPLAY_MAC[6]    = {0x94, 0xA9, 0x90, 0x30, 0x3B, 0xCC};  // board B: display

// Message types. Explicit values, because they are sent over the air
enum MsgType : uint8_t {
  Msg_Im_here = 1,  // heartbeat
  Msg_Push    = 2,  // button pressed
};

// The message both boards send and receive over ESP-NOW
struct GameMsg {
  uint8_t type;   // one of MsgType
  uint32_t seq;   // sequence number, +1 on every message
};
