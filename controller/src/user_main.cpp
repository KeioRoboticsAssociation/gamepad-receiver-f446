#include "user_main.h"

#include <memory>
#include <stddef.h>
#include <stdint.h>

#include <stm32f4xx_hal.h>
#include <can.h>
#include <gpio.h>
#include <tim.h>
#include <usb_host.h>
#include <usbh_hid.h>

#include "report_parser.h"

namespace {
  const uint32_t CAN_ID = 0x334;

  ApplicationTypeDef stateA;
  HOST_StateTypeDef stateH;
  std::unique_ptr<ReportParser> parser;
  std::unique_ptr<uint8_t[]> reportBuf;
  bool ready = false;
  bool received = false;
  CAN_TxHeaderTypeDef canHeader = {CAN_ID, 0, CAN_RTR_DATA, CAN_ID_STD, 0, DISABLE};
  uint32_t canMailbox;
}

extern TIM_HandleTypeDef htim6;
extern USBH_HandleTypeDef hUsbHostFS;
extern ApplicationTypeDef Appli_state;

void onTimer6() { // 60fps
  if (received) {
    auto HidHandle = (HID_HandleTypeDef*) hUsbHostFS.pActiveClass->pData;
    fifo_read(&(HidHandle->fifo), reportBuf.get(), parser->reportLength);
    received = false;
  } else {
    USBH_HID_GetReport(&hUsbHostFS, 0x01, parser->reportId, reportBuf.get(), parser->reportLength);
  }
  printf("Report: ");
  for (size_t i = 0; i < parser->reportLength; i++) {
    for (uint8_t bit = 0x01; bit != 0; bit <<= 1) {
      putchar(reportBuf[i] & bit ? '1' : '0');
    }
    putchar(' ');
  }
  parser->parse(reportBuf.get(), parser->reportLength);
  //printf(parser->axes.x == 0x0f ? "     " : "%4d ", parser->axes.x);
  printf("Axes: %4d %4d %4d %4d ", parser->axes.x, parser->axes.y, parser->axes.z, parser->axes.rz);
  printf("Buttons: ");
  for (auto button : parser->buttons) {
    putchar(button ? '1' : '0');
  }
  putchar('\n');
  uint8_t canData[8] = {};
  canData[0] = (uint8_t) parser->axes.x;
  canData[1] = (uint8_t) parser->axes.y;
  canData[2] = (uint8_t) parser->axes.z;
  canData[3] = (uint8_t) parser->axes.rz;
  for (size_t i = 0; i < canHeader.DLC - 4; i++) {
    uint8_t data = 0;
    for (size_t j = 0; j < 8 && i * 8 + j < parser->buttons.size(); j++) {
      data |= (uint8_t) parser->buttons[i * 8 + j] << j;
    }
    canData[i + 4] = data;
  }
  //if (HAL_CAN_IsTxMessagePending(&hcan1, canMailbox)) {
  while (HAL_CAN_IsTxMessagePending(&hcan1, canMailbox)) {
    printf("CAN Abort\n");
    HAL_CAN_AbortTxRequest(&hcan1, canMailbox);
  }
  HAL_CAN_AddTxMessage(&hcan1, &canHeader, canData, &canMailbox);
}

void USBH_HID_EventCallback(USBH_HandleTypeDef* phost) {
  if (!ready) {
    ready = true;
    const auto dataLength = 4 + parser->buttons.size() / 8 + (parser->buttons.size() % 8 ? 1 : 0);
    canHeader.DLC = dataLength < 8 ? dataLength : 8;
    HAL_CAN_Start(&hcan1);
    HAL_TIM_Base_Start_IT(&htim6);
  }
  received = true;
}

void userInit() {
  stateA = Appli_state;
  stateH = hUsbHostFS.gState;
  printf("Start\n");
  // printf("H: %d, ", stateH);
  // printf("A: %d\n", stateA);
}

void userMain() {
  if (Appli_state != stateA || hUsbHostFS.gState != stateH) {
    // printf("H: %d, ", hUsbHostFS.gState);
    // printf("A: %d\n", Appli_state);
    if (Appli_state != stateA && Appli_state == APPLICATION_READY) {
      printf("Connected\n");
      auto HidHandle = (HID_HandleTypeDef*) hUsbHostFS.pActiveClass->pData;
      printf("Report Descriptor: ");
      for (size_t i = 0; i < HidHandle->HID_Desc.wItemLength; i++) {
        printf("%02x", hUsbHostFS.device.Data[i]);
      }
      putchar('\n');
      parser.reset(new ReportParser(hUsbHostFS.device.Data, HidHandle->HID_Desc.wItemLength));
      if (HidHandle->length > parser->reportLength / 4) {
        HidHandle->length = parser->reportLength / 4;
      }
      reportBuf.reset(new uint8_t[parser->reportLength]);
      HidHandle->pData = reportBuf.get();
      fifo_init(&(HidHandle->fifo), hUsbHostFS.device.Data, HID_QUEUE_SIZE * parser->reportLength);
      // USBH_HID_SetIdle(&hUsbHostFS, 0x04, parser->reportId); // Not working...
      // USBH_HID_SetIdle(&hUsbHostFS, 0x04, 0);
      HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
    } else if (Appli_state != stateA && Appli_state == APPLICATION_DISCONNECT) {
      ready = false;
      HAL_CAN_Stop(&hcan1);
      HAL_TIM_Base_Stop_IT(&htim6);
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
