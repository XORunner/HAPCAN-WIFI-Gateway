#include "hapcan.h"
#include "common.h"
#include "oled.h"
#include "clients.h"

// The global clients array is defined in main.ino.
// We declare it as extern here so that broadcastFrame() can access it.
extern struct SocketConnection clients[MAX_CLIENTS];

// encodeFrame(): Encodes a TWAI (CAN) message into a 15-byte HAPCAN frame.
size_t encodeFrame(const twai_message_t &msg, uint8_t *outFrame) {
  outFrame[0] = HAPCAN_START_BYTE;
  outFrame[1] = (msg.identifier & 0x1fe00000UL) >> 21;
  outFrame[2] = ((msg.identifier & 0x1e0000UL) >> 13) | ((msg.identifier & 0x10000UL) >> 16);
  outFrame[3] = (msg.identifier & 0xff00UL) >> 8;
  outFrame[4] = (msg.identifier & 0xffUL);
  
  for (int i = 0; i < 8; i++) {
    if (i < msg.data_length_code) {
      outFrame[5 + i] = msg.data[i];
    } else {
      outFrame[5 + i] = 0;
    }
  }
  
  uint16_t sum = 0;
  for (int i = 1; i < 13; i++) {
    sum += outFrame[i];
  }
  outFrame[13] = sum % 256;
  outFrame[14] = HAPCAN_END_BYTE;
  return 15;
}

// decodeFrame(): Decodes a 15-byte HAPCAN frame into a TWAI message.
bool decodeFrame(const uint8_t* frame, size_t len, twai_message_t &msg) {
  if (len != 15) return false;
  uint32_t id = 0;
  id |= ((uint32_t)frame[1]) << 21;
  id |= ((uint32_t)(frame[2] & 0xF0)) << 13;
  id |= ((uint32_t)(frame[2] & 0x01)) << 16;
  id |= ((uint32_t)frame[3]) << 8;
  id |= frame[4];
  
  msg.identifier = id;
  msg.extd = true;
  msg.data_length_code = 8;
  for (int i = 0; i < 8; i++) {
    msg.data[i] = frame[5 + i];
  }
  return true;
}

// processFrame(): Processes an incoming HAPCAN frame.
// It prints a debug message, updates the OLED (using addHAPCANDisplayMessage),
// and, if applicable, transmits a corresponding CAN message.
void processFrame(const uint8_t* frame, size_t len) {
  String debugMsg = "-> ";
  for (size_t i = 0; i < len; i++) {
    if (frame[i] < 16) debugMsg += "0";
    debugMsg += String(frame[i], HEX) + ":";
  }
  debugPrintln(debugMsg);
  
  if (len == 15) {
    // For TCP→CAN, update the OLED display.
    addHAPCANDisplayMessage(true, frame, len);
    twai_message_t tx_msg;
    if (decodeFrame(frame, len, tx_msg)) {
      esp_err_t err = twai_transmit(&tx_msg, pdMS_TO_TICKS(10));
      if (err != ESP_OK) {
        debugPrintln("Failed to transmit CAN message: " + String(err));
      }
    }
  } else if (len == 5 && frame[1] == 0x10) {
    uint8_t cmd = frame[2];
    if (cmd == 0x40) {
      uint8_t response[] = {HAPCAN_START_BYTE, 0x10, 0x41, 0x30, 0x00, 0x03, 0xFF,
                              0x00, 0x00, 0x07, 0xA0, 0x2A, HAPCAN_END_BYTE};
      broadcastFrame(response, sizeof(response));
      debugPrintln("Hardware type request processed");
    } else if (cmd == 0x60) {
      uint8_t response[] = {HAPCAN_START_BYTE, 0x10, 0x61, 0x30, 0x00, 0x03, 0x65,
                              0x00, 0x00, 0x03, 0x04, 0x10, HAPCAN_END_BYTE};
      broadcastFrame(response, sizeof(response));
      debugPrintln("Firmware type request processed");
    } else if (cmd == 0xE0) {
      uint8_t response1[] = {HAPCAN_START_BYTE, 0x10, 0xE1, 0x52, 0x53, 0x32, 0x33,
                               0x32, 0x43, 0x20, 0x49, 0xD9, HAPCAN_END_BYTE};
      uint8_t response2[] = {HAPCAN_START_BYTE, 0x10, 0xE1, 0x6E, 0x74, 0x65, 0x72,
                               0x66, 0x61, 0x63, 0x65, 0x39, HAPCAN_END_BYTE};
      broadcastFrame(response1, sizeof(response1));
      broadcastFrame(response2, sizeof(response2));
      debugPrintln("Description request processed");
    } else if (cmd == 0xC0) {
      uint8_t response[] = {HAPCAN_START_BYTE, 0x10, 0xC1, 0xC5, 0x40, 0xA7, 0x70,
                              0xFF, 0xFF, 0xFF, 0xFF, 0xE9, HAPCAN_END_BYTE};
      broadcastFrame(response, sizeof(response));
      debugPrintln("Supply voltage request processed");
    } else {
      debugPrintln("Unknown command received");
    }
  } else {
    debugPrintln("Unknown HAPCAN packet length: " + String(len));
  }
}

// broadcastFrame(): Sends a frame to all active TCP clients.
void broadcastFrame(const uint8_t* data, size_t len) {
  extern SocketConnection clients[MAX_CLIENTS];
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (clients[i].active && clients[i].client.connected()) {
      clients[i].client.write(data, len);
    }
  }
}
