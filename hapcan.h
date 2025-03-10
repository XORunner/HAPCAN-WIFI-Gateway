#ifndef HAPCAN_H
#define HAPCAN_H

#include <Arduino.h>
#include "driver/twai.h"
#include "common.h"
#include "oled.h"   // For addHAPCANDisplayMessage() and debugPrintln()

// HAPCAN-related function prototypes.
size_t encodeFrame(const twai_message_t &msg, uint8_t *outFrame);
bool decodeFrame(const uint8_t* frame, size_t len, twai_message_t &msg);
void processFrame(const uint8_t* frame, size_t len);
void broadcastFrame(const uint8_t* data, size_t len);

#endif
