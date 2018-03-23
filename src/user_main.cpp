#include "user_main.h"

#include "stm32f4xx_hal.h"
#include "usb_host.h"
#include "gpio.h"

#include "usbh_hid.h"

namespace {
  uint8_t reportBuf[95]; // TODO: Buf size less than 95 doesn't work, but idk why. (Boundary value changed from 104 to 95)
  ApplicationTypeDef stateA;
  HOST_StateTypeDef stateH;
  int flag = 0;
}

extern USBH_HandleTypeDef hUsbHostFS;
extern ApplicationTypeDef Appli_state;

void USBH_HID_EventCallback(USBH_HandleTypeDef* phost) {
  // printf("B\n");
  // fifo_read(&(((HID_HandleTypeDef *) phost->pActiveClass->pData)->fifo), reportBuf, 6);
  fifo_read(&(((HID_HandleTypeDef*) phost->pActiveClass->pData)->fifo), reportBuf, 8);
  printf("Report: ");
  // for (size_t i = 0; i < 6; i++) {
  for (size_t i = 0; i < 8; i++) {
    // printf("%02x", reportBuf[i]);
    for (uint8_t bit = 0x01; bit != 0; bit <<= 1) {
      putchar(reportBuf[i] & bit ? '1' : '0');
    }
    putchar(' ');
  }
  putchar('\n');
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, reportBuf[4] & 0x01 << 5 ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void userInit() {
  ApplicationTypeDef stateA = Appli_state;
  HOST_StateTypeDef stateH = hUsbHostFS.gState;
  printf("Start\n");
  printf("H: %d, ", stateH);
  printf("A: %d\n", stateA);
}

void userMain() {
  if (Appli_state != stateA || hUsbHostFS.gState != stateH) {
    stateA = Appli_state;
    stateH = hUsbHostFS.gState;
    printf("H: %d, ", stateH);
    printf("A: %d\n", stateA);
  }
  if (stateA == APPLICATION_READY && !flag) {
    printf("Connected\n");
    flag = 1;
    size_t reportDescLength = ((HID_HandleTypeDef*) hUsbHostFS.pActiveClass->pData)->HID_Desc.wItemLength;
    printf("Report Descriptor: ");
    for (size_t i = 0; i < reportDescLength; i++) {
      printf("%02x", hUsbHostFS.device.Data[i]);
    }
    putchar('\n');
    // ((HID_HandleTypeDef *) hUsbHostFS.pActiveClass->pData)->length = 6;
    ((HID_HandleTypeDef*) hUsbHostFS.pActiveClass->pData)->length = 8;
    fifo_init(&(((HID_HandleTypeDef*) hUsbHostFS.pActiveClass->pData)->fifo), hUsbHostFS.device.Data, 32);
    // printf("A\n");
  //} else if (stateH == HOST_ABORT_STATE && !flag) {
    //  printf("abort\n");
    //  flag = 1;
  } else if (Appli_state == APPLICATION_DISCONNECT && flag) {
    printf("Disconnected\n");
    flag = 0;
  }
}
