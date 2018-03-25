#include "user_main.h"

#include <memory>

#include <stm32f4xx_hal.h>
#include <usb_host.h>
#include <gpio.h>
#include <usbh_hid.h>

#include "report_parser.h"

namespace {
  ApplicationTypeDef stateA;
  HOST_StateTypeDef stateH;
  std::unique_ptr<ReportParser> parser;
  std::unique_ptr<uint8_t[]> reportBuf;
}

extern USBH_HandleTypeDef hUsbHostFS;
extern ApplicationTypeDef Appli_state;

void USBH_HID_EventCallback(USBH_HandleTypeDef* phost) {
  auto HidHandle = (HID_HandleTypeDef*) hUsbHostFS.pActiveClass->pData;
  fifo_read(&(HidHandle->fifo), reportBuf.get(), parser->reportLength);
  printf("Report: ");
  for (size_t i = 0; i < parser->reportLength; i++) {
    for (uint8_t bit = 0x01; bit != 0; bit <<= 1) {
      putchar(reportBuf[i] & bit ? '1' : '0');
    }
    putchar(' ');
  }
  putchar('\n');
  // HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, reportBuf[4] & 0x01 << 5 ? GPIO_PIN_SET : GPIO_PIN_RESET);
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
    printf("H: %d, ", hUsbHostFS.gState);
    printf("A: %d\n", Appli_state);
    if (Appli_state != stateA && Appli_state == APPLICATION_READY) {
      printf("Connected\n");
      auto HidHandle = (HID_HandleTypeDef*) hUsbHostFS.pActiveClass->pData;
      parser.reset(new ReportParser(hUsbHostFS.device.Data, HidHandle->HID_Desc.wItemLength));
      if (HidHandle->length > parser->reportLength / 4) {
        HidHandle->length = parser->reportLength / 4;
      }
      reportBuf.reset(new uint8_t[parser->reportLength]);
      HidHandle->pData = reportBuf.get();
      fifo_init(&(HidHandle->fifo), hUsbHostFS.device.Data, HID_QUEUE_SIZE * parser->reportLength);
      HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
    } else if (Appli_state != stateA && Appli_state == APPLICATION_DISCONNECT) {
      printf("Disconnected\n");
      parser.reset();
      reportBuf.reset();
      HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
    } else if (hUsbHostFS.gState != stateH && hUsbHostFS.gState == HOST_ABORT_STATE) {
      printf("Abort\n");
    }
    stateA = Appli_state;
    stateH = hUsbHostFS.gState;
  }
}
