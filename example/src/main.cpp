#include "main.h"

#include <mbed.h>

#include "controller.h"

namespace {
  DigitalOut led(LED1, false);
  CAN can1(PA_11, PA_12, 500000);
  Serial pc(USBTX, USBRX, 115200);

  Controller controller(can1, 0x334);
}

int main(){
  wait(0.5);
  pc.printf("start");
  led = true;
  while (true) {
    pc.printf("Axes: %4d %4d %4d %4d ", controller.axes.x, controller.axes.y, controller.axes.z, controller.axes.rz);
    pc.printf("Buttons: ");
    for (auto button : controller.buttons) {
      pc.putc(button ? '1' : '0');
    }
    pc.putc('\n');
    wait(0.0166);
  }
}
