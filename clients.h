#ifndef CLIENTS_H
#define CLIENTS_H

#include <WiFi.h>
#include "common.h"

struct SocketConnection {
  WiFiClient client;
  class HapcanParser {
  public:
    enum State { WAIT, COLLECTING };
    State state;
    uint8_t buffer[16];
    size_t index;
    
    HapcanParser() { reset(); }
    void reset() { state = WAIT; index = 0; }

bool parseByte(uint8_t byte) {
  if (state == WAIT) {
    if (byte == HAPCAN_START_BYTE) {
      buffer[0] = byte;
      index = 1;
      state = COLLECTING;
    }
  } else if (state == COLLECTING) {
    if (index < sizeof(buffer)) {
      buffer[index++] = byte;
      // Check if we have a complete valid frame at one of the valid lengths.
      if ((index == 5 || index == 13 || index == 15) && buffer[index - 1] == HAPCAN_END_BYTE) {
        state = WAIT;
        return true;
      }
      if (index > 15) {
        reset();
      }
    } else {
      reset();
    }
  }
  return false;
}


    uint8_t* getFrame() { return buffer; }
    size_t getFrameLength() { return index; }
  } parser;
  bool active;
};

extern SocketConnection clients[MAX_CLIENTS];

#endif
