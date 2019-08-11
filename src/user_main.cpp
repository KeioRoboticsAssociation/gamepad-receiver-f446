#include "user_main.h"

#include <memory>
#include <inttypes.h>
#include <stddef.h>

#include "stm32f4xx_hal.h"
#include "usb_host.h"
#include "usbh_hid.h"

#include "report_parser.h"

extern CAN_HandleTypeDef hcan1;
extern TIM_HandleTypeDef htim6;
extern UART_HandleTypeDef huart2;
extern USBH_HandleTypeDef hUsbHostFS;
extern ApplicationTypeDef Appli_state;

namespace {
  enum struct MainState {
    IDLE,
    CONNECTED,
    INIT_DETECT_POLLING_5,
    INIT_DETECT_POLLING_4,
    INIT_DETECT_POLLING_3,
    INIT_DETECT_POLLING_2,
    INIT_DETECT_POLLING_1,
    READY,
  };

  constexpr uint32_t CAN_ID = 0x334;

  struct {
    struct {
      HOST_StateTypeDef host;
      ApplicationTypeDef app;
    } usb;
    struct {
      MainState main = MainState::IDLE;
      bool received = false;
      bool polling = true;
    } app;
  } state = {};
  std::unique_ptr<ReportParser> parser;
  std::unique_ptr<uint8_t[]> reportBuf;
  CAN_TxHeaderTypeDef canHeader = {CAN_ID, 0, CAN_RTR_DATA, CAN_ID_STD, 0, DISABLE};
  uint32_t canMailbox;
}

void onTimer6() { // 60fps
  if (state.app.received) {
    printf("R ");
    auto hidHandle = reinterpret_cast<HID_HandleTypeDef*>(hUsbHostFS.pActiveClass->pData);
    USBH_HID_FifoRead(&(hidHandle->fifo), reportBuf.get(), parser->reportLength);
  } else if (state.app.polling) {
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
  switch (state.app.main) {
    case MainState::READY: {
      parser->parse(reportBuf.get());
      printf("Axes: %4d %4d %4d %4d ", parser->axes.x, parser->axes.y, parser->axes.z, parser->axes.rz);
      printf("Buttons: ");
      for (auto button : parser->buttons) {
        putchar(button ? '1' : '0');
      }
      uint8_t canData[8] = {};
      canData[0] = parser->axes.x;
      canData[1] = parser->axes.y;
      canData[2] = parser->axes.z;
      canData[3] = parser->axes.rz;
      const size_t buttonsNum = parser->buttons.size() <= 24 ? parser->buttons.size() : 24;
      canData[4] = buttonsNum;
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
      canHeader.DLC = 8;
      HAL_CAN_Start(&hcan1);
    case MainState::INIT_DETECT_POLLING_2:
    case MainState::INIT_DETECT_POLLING_3:
    case MainState::INIT_DETECT_POLLING_4:
    case MainState::INIT_DETECT_POLLING_5:
      state.app.main = static_cast<MainState>(static_cast<int>(state.app.main) + 1);
      printf("Detecting whether polling is needed\n");
      if (state.app.received) {
        state.app.polling = false;
      }
      break;
    case MainState::CONNECTED:
      state.app.main = MainState::INIT_DETECT_POLLING_5;
      printf("Drop first report\n");
      break;
    default:
      break;
  }
  state.app.received = false;
}

void USBH_HID_EventCallback(USBH_HandleTypeDef* phost) {
  state.app.received = true;
}

void setup() {
  state.usb.host = hUsbHostFS.gState;
  state.usb.app = Appli_state;
  printf("gamepad-receiver-f446 v1.2.0 @Piroro-hs\nhttps://github.com/KeioRoboticsAssociation/gamepad-receiver-f446\n");
}

void loop() {
  if (state.usb.host != hUsbHostFS.gState) {
    state.usb.host = hUsbHostFS.gState;
    switch (state.usb.host) {
      case HOST_ABORT_STATE:
        printf("Abort\n");
        break;
      default:
        break;
    }
  }
  if (state.usb.app != Appli_state) {
    state.usb.app = Appli_state;
    switch (state.usb.app) {
    case APPLICATION_READY: {
      state.app.main = MainState::CONNECTED;
      printf("Connected\n");
      auto hidHandle = reinterpret_cast<HID_HandleTypeDef*>(hUsbHostFS.pActiveClass->pData);
      printf("Report Descriptor: ");
      for (size_t i = 0; i < hidHandle->HID_Desc.wItemLength; i++) {
        printf("%02x", hUsbHostFS.device.Data[i]);
      }
      putchar('\n');
      parser.reset(new ReportParser(hUsbHostFS.device.Data, hidHandle->HID_Desc.wItemLength));
      reportBuf.reset(new uint8_t[parser->reportLength]);
      hidHandle->length = parser->reportLength;
      hidHandle->pData = reportBuf.get();
      USBH_HID_FifoInit(&(hidHandle->fifo), hUsbHostFS.device.Data, HID_QUEUE_SIZE * parser->reportLength);
      HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
      HAL_TIM_Base_Start_IT(&htim6);
      break;
    }
    case APPLICATION_DISCONNECT:
      state.app = {};
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
