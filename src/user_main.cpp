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

  enum struct MainState {
    IDLE,
    CONNECTED,
    INIT_FIRST_RECIEVED,
    INIT_DETECT_POLLING_5,
    INIT_DETECT_POLLING_4,
    INIT_DETECT_POLLING_3,
    INIT_DETECT_POLLING_2,
    INIT_DETECT_POLLING_1,
    READY,
  };
  struct {
    HOST_StateTypeDef usbHost;
    ApplicationTypeDef usbApp;
    MainState main = MainState::IDLE;
    bool received = false;
    bool polling = true;
  } state;
  std::unique_ptr<ReportParser> parser;
  std::unique_ptr<uint8_t[]> reportBuf;
  CAN_TxHeaderTypeDef canHeader = {CAN_ID, 0, CAN_RTR_DATA, CAN_ID_STD, 0, DISABLE};
  uint32_t canMailbox;
}

extern USBH_HandleTypeDef hUsbHostFS;
extern ApplicationTypeDef Appli_state;

void onTimer6() { // 60fps
  if (state.received) {
    printf("R ");
    auto HidHandle = (HID_HandleTypeDef*) hUsbHostFS.pActiveClass->pData;
    fifo_read(&(HidHandle->fifo), reportBuf.get(), parser->reportLength);
  } else if (state.polling) {
    printf("P ");
    USBH_HID_GetReport(&hUsbHostFS, 0x01, parser->reportId, reportBuf.get(), parser->reportLength);
  } else {
    printf("N ");
  }
  printf("Report: ");
  for (size_t i = 0; i < parser->reportLength; i++) {
    for (uint8_t bit = 0x01; bit != 0; bit <<= 1) {
      putchar(reportBuf[i] & bit ? '1' : '0');
    }
    putchar(' ');
  }
  switch (state.main) {
  case MainState::READY: {
    parser->parse(reportBuf.get());
    printf("Axes: %4d %4d %4d %4d ", parser->axes.x, parser->axes.y, parser->axes.z, parser->axes.rz);
    printf("Buttons: ");
    for (auto button : parser->buttons) {
      putchar(button ? '1' : '0');
    }
    uint8_t canData[8] = {};
    canData[0] = (uint8_t) parser->axes.x;
    canData[1] = (uint8_t) parser->axes.y;
    canData[2] = (uint8_t) parser->axes.z;
    canData[3] = (uint8_t) parser->axes.rz;
    const size_t buttonsNum = parser->buttons.size() <= 24 ? parser->buttons.size() : 24;
    canData[4] = (uint8_t) buttonsNum;
    for (size_t i = 0; i < buttonsNum; i++) {
      if (parser->buttons[i]) {
        canData[5 + i / 8] |= 0x01 << (7 - i % 8);
      }
    }
    printf(" CAN:");
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0) {
      HAL_CAN_AbortTxRequest(&hcan1, canMailbox);
      printf(" Abort");
    }
    putchar('\n');
    HAL_CAN_AddTxMessage(&hcan1, &canHeader, canData, &canMailbox);
    break;
  }
  case MainState::INIT_DETECT_POLLING_1:
    // const auto dataLength = 5 + parser->buttons.size() / 8 + (parser->buttons.size() % 8 ? 1 : 0);
    // canHeader.DLC = dataLength < 8 ? dataLength : 8;
    canHeader.DLC = 8;
    HAL_CAN_Start(&hcan1);
  case MainState::INIT_DETECT_POLLING_2:
  case MainState::INIT_DETECT_POLLING_3:
  case MainState::INIT_DETECT_POLLING_4:
  case MainState::INIT_DETECT_POLLING_5:
    state.main = (MainState) ((uint8_t) state.main + 1);
    printf("Detecting whether polling is needed\n");    
    if (state.received) {
      state.polling = false;
    }
    break;
  case MainState::INIT_FIRST_RECIEVED:
    state.main = MainState::INIT_DETECT_POLLING_5;
    printf("Drop first report\n");
    break;
  default:
    break;
  }
  state.received = false;
}

void USBH_HID_EventCallback(USBH_HandleTypeDef* phost) {
  state.received = true;
  if (state.main == MainState::CONNECTED) {
    state.main = MainState::INIT_FIRST_RECIEVED;
    HAL_TIM_Base_Start_IT(&htim6);
    // HAL_Delay(100);
    // state.main = MainState::READY;
    // const auto dataLength = 5 + parser->buttons.size() / 8 + (parser->buttons.size() % 8 ? 1 : 0);
    // canHeader.DLC = dataLength < 8 ? dataLength : 8;
    // canHeader.DLC = 8;
    // HAL_CAN_Start(&hcan1);
  }
}

void userInit() {
  state.usbHost = hUsbHostFS.gState;
  state.usbApp = Appli_state;
  printf("gamepad-receiver-f446 v1.2.0 @Piroro-hs\nhttps://github.com/KeioRoboticsAssociation/gamepad-receiver-f446\n");
  // printf("H: %d, ", stateH);
  // printf("A: %d\n", stateA);
}

void userMain() {
  if (state.usbHost == hUsbHostFS.gState && state.usbApp == Appli_state) {
    return;
  }
  if (state.usbHost != hUsbHostFS.gState) {
    state.usbHost = hUsbHostFS.gState;
    switch (state.usbHost) {
    case HOST_ABORT_STATE:
      printf("Abort\n");
      break;
    default:
      break;
    }
  }
  if (state.usbApp != Appli_state) {
    state.usbApp = Appli_state;
    switch (state.usbApp) {
    case APPLICATION_READY: {
      state.main = MainState::CONNECTED;
      printf("Connected\n");
      auto HidHandle = (HID_HandleTypeDef*) hUsbHostFS.pActiveClass->pData;
      printf("Report Descriptor: ");
      for (size_t i = 0; i < HidHandle->HID_Desc.wItemLength; i++) {
        printf("%02x", hUsbHostFS.device.Data[i]);
      }
      putchar('\n');
      parser.reset(new ReportParser(hUsbHostFS.device.Data, HidHandle->HID_Desc.wItemLength));
      reportBuf.reset(new uint8_t[parser->reportLength]);
      HidHandle->length = HidHandle->length > parser->reportLength / 4 ? parser->reportLength / 4 : HidHandle->length;
      HidHandle->pData = reportBuf.get();
      fifo_init(&(HidHandle->fifo), hUsbHostFS.device.Data, HID_QUEUE_SIZE * parser->reportLength);
      HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
      break;
    }
    case APPLICATION_DISCONNECT:
      state.main = MainState::IDLE;
      state.received = false;
      state.polling = true;
      printf("Disconnected\n");
      HAL_CAN_Stop(&hcan1);
      HAL_TIM_Base_Stop_IT(&htim6);
      parser.reset();
      reportBuf.reset();
      HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
      break;
    default:
      break;
    }
  }
}
