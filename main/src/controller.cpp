#include "controller.h"

#include <stddef.h>
#include <stdint.h>

#include <mbed.h>

Controller::Controller(CAN& _can, const uint32_t _canId) : can(_can), canId(_canId) {
  can.attach(callback(this, &Controller::recieveData));
}

void Controller::setButtonEventListener(Callback<void(size_t, bool)> cb) {
  buttonCallback = cb;
}

void Controller::recieveData() {
  CANMessage msg;
  if(can.read(msg) && msg.id == canId && msg.type == CANData){
    //for (size_t i = 0; i < 8; i++) {
      //raw[i] = msg.data[i];
    //]
    parse(msg.data, msg.len);
  }
  //received = true;
}

void Controller::parse(const uint8_t* data, const size_t length) {
  axes.x = (int8_t) data[0];
  axes.y = (int8_t) data[1];
  axes.z = (int8_t) data[2];
  axes.rz = (int8_t) data[3];
  buttons.resize(data[4], false);
  for (size_t i = 0; i < data[4]; i++) {
    const bool next = data[4 + i / 8] && 0x80 >> i % 8;
    if (buttons[i] != next && buttonCallback) {
      buttonCallback(i, next);
    }
    buttons[i] = next;
  }
}
