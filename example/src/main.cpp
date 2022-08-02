#include "main.h"

#include <mbed.h>

#include "controller.h"

namespace {
DigitalOut led(LED1, false);
RawCAN can1(PA_11, PA_12, 500000);

Controller controller(can1, 0x334);
}  // namespace

int main() {
  ThisThread::sleep_for(500ms);
  printf("start");
  led = true;
  while (true) {
    printf("Axes: %4d %4d %4d %4d ", controller.axes.x, controller.axes.y,
           controller.axes.z, controller.axes.rz);
    printf("Buttons: ");
    for (auto button : controller.buttons) {
      printf("%d", (uint8_t)button);
    }
    printf("\n");
    wait_us(16600);
  }
}
