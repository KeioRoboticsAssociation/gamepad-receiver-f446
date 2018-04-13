#include "report_parser.h"

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include <usbh_hid.h>
#include <usbh_hid_usage.h>

ReportParser::ReportParser(const uint8_t* reportDescriptor, const size_t length) {
  size_t buttonNum = 0;
  size_t reportSize = 0;
  struct {
    uint32_t usagePage = 0; // Global
    int32_t logicalMin = 0;
    int32_t logicalMax = 0;
    uint32_t reportSize = 0;
    uint32_t reportCount = 0;
    std::vector<uint32_t> usages = {}; // Local
    // uint32_t usageMin = 0;
    // uint32_t usageMax = 0;
  } current;
  size_t i = 0;
  while (i < length) {
    const auto byte = reportDescriptor[i];
    const auto bSize = byte & 0x03;
    // const bType = byte >> 2 & 0x03;
    // const bTag = bite >> 4;
    const auto dataLength = bSize == 3 ? 4 : bSize;
    const auto type = byte >> 2;
    switch (type) {
    case HID_MAIN_ITEM_TAG_INPUT << 2 | HID_ITEM_TYPE_MAIN:
      if (current.usagePage == HID_USAGE_PAGE_BUTTON) {
        for (size_t j = 0; j < current.reportCount; j++) {
          controls.push_back(Control {
            ControlType::BUTTON,
            reportSize,
            current.reportSize,
            current.logicalMin,
            current.logicalMax,
            current.logicalMin
          });
          buttonNum++;
          reportSize += current.reportSize;
        }
      } else {
        for (auto usage : current.usages) {
          controls.push_back(Control {
            usage == HID_USAGE_X ? ControlType::JOYSTICK_X :
            usage == HID_USAGE_Y ? ControlType::JOYSTICK_Y :
            usage == HID_USAGE_Z ? ControlType::JOYSTICK_Z :
            usage == HID_USAGE_RZ ? ControlType::JOYSTICK_Rz :
            usage == HID_USAGE_HATSW ? ControlType::HATSWITCH : ControlType::NOOP,
            reportSize,
            current.reportSize,
            current.logicalMin,
            current.logicalMax,
            current.logicalMin
          });
          reportSize += current.reportSize;
        }
      }
    case HID_MAIN_ITEM_TAG_OUTPUT << 2 | HID_ITEM_TYPE_MAIN:
    case HID_MAIN_ITEM_TAG_COLLECTION << 2 | HID_ITEM_TYPE_MAIN:
    case HID_MAIN_ITEM_TAG_FEATURE << 2 | HID_ITEM_TYPE_MAIN:
    case HID_MAIN_ITEM_TAG_ENDCOLLECTION << 2 | HID_ITEM_TYPE_MAIN:
      current.usages = {};
      break;
    case HID_GLOBAL_ITEM_TAG_REPORT_SIZE << 2 | HID_ITEM_TYPE_GLOBAL:
      current.reportSize = readReportDescriptorData(reportDescriptor, i + 1, dataLength);
      break;
    case HID_GLOBAL_ITEM_TAG_REPORT_ID << 2 | HID_ITEM_TYPE_GLOBAL:
      if (reportId) {
        i = length; // break
      } else {
        controls.push_back(Control {ControlType::NOOP, reportSize, 8});
        reportSize += 8;
        reportId = (uint8_t) readReportDescriptorData(reportDescriptor, i + 1, dataLength);
      }
      break;
    case HID_GLOBAL_ITEM_TAG_REPORT_COUNT << 2 | HID_ITEM_TYPE_GLOBAL:
      current.reportCount = readReportDescriptorData(reportDescriptor, i + 1, dataLength);
      break;
    case HID_GLOBAL_ITEM_TAG_LOG_MIN << 2 | HID_ITEM_TYPE_GLOBAL:
      current.logicalMin = castToSigned(readReportDescriptorData(reportDescriptor, i + 1, dataLength), dataLength * 8);
      break;
    case HID_GLOBAL_ITEM_TAG_LOG_MAX << 2 | HID_ITEM_TYPE_GLOBAL:
      current.logicalMax = castToSigned(readReportDescriptorData(reportDescriptor, i + 1, dataLength), dataLength * 8);
      break;
    case HID_GLOBAL_ITEM_TAG_USAGE_PAGE << 2 | HID_ITEM_TYPE_GLOBAL:
      current.usagePage = readReportDescriptorData(reportDescriptor, i + 1, dataLength);
      break;
    case HID_LOCAL_ITEM_TAG_USAGE << 2 | HID_ITEM_TYPE_LOCAL:
      current.usages.push_back(readReportDescriptorData(reportDescriptor, i + 1, dataLength));
      break;
    default:
      break;
    }
    i += dataLength + 1;
  }
  buttons = std::vector<bool>(buttonNum > 12 ? buttonNum + 4 : 16, false);
  reportLength = reportSize / 8;
}

void ReportParser::parse(const uint8_t* report, const size_t length) {
  size_t buttonIndex = 0;
  for (auto &control : controls) {
    const auto data = readReportData(report, control.index, control.size, control.min < 0);
    if (first) {
      control.neutral = data;
    }
    switch (control.type) {
    case ControlType::JOYSTICK_X:
      axes.x = (data - control.neutral) * 100 / (data >= control.neutral ? control.max - control.neutral : control.neutral - control.min);
      break;
    case ControlType::JOYSTICK_Y:
      axes.y = (data - control.neutral) * 100 / (data >= control.neutral ? control.max - control.neutral : control.neutral - control.min);
      break;
    case ControlType::JOYSTICK_Z:
      axes.z = (data - control.neutral) * 100 / (data >= control.neutral ? control.max - control.neutral : control.neutral - control.min);
      break;
    case ControlType::JOYSTICK_Rz:
      axes.rz = (data - control.neutral) * 100 / (data >= control.neutral ? control.max - control.neutral : control.neutral - control.min);
      break;
    case ControlType::HATSWITCH:
      buttons[12] = data - control.min == 7 || data - control.min == 0 || data - control.min == 1;
      buttons[15] = data - control.min >= 1 && data - control.min <= 3;
      buttons[13] = data - control.min >= 3 && data - control.min <= 5;
      buttons[14] = data - control.min >= 5 && data - control.min <= 7;
      break;
    case ControlType::BUTTON:
      buttons[buttonIndex++] = control.neutral != data;
      if (buttonIndex == 12) {
        buttonIndex = 16;
      }
      break;
    default:
      break;
    }
  }
  if (first) {
    first = false;
  }
}

uint32_t ReportParser::readReportDescriptorData(
  const uint8_t* reportDescriptor,
  const size_t index,
  const size_t length
) {
  uint32_t data = 0;
  for (size_t i = index + length - 1; i >= index; i--) {
    (data <<= 8) |= reportDescriptor[i];
  }
  return data;
}

int32_t ReportParser::readReportData(
  const uint8_t* report,
  const size_t index,
  const size_t size,
  const bool isSigned
) {
  uint32_t data = 0;
  for (size_t i = 0; i < size; i++) {
    data |= (uint32_t) (report[(i + index) / 8] >> (i + index) % 8 & 0x01) << i;
  }
  return isSigned ? castToSigned(data, size) : (int32_t) data;
}

int32_t ReportParser::castToSigned(const uint32_t data, const size_t size) {
  return data >> (size - 1) ? (int32_t) (data << (32 - size)) >> (32 - size) : (int32_t) data;
}
