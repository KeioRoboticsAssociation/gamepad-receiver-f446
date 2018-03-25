#include "report_parser.h"

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include <usbh_hid.h>
#include <usbh_hid_usage.h>

ReportParser::ReportParser(const uint8_t* reportDescriptor, const size_t length) {
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
    const uint8_t byte = reportDescriptor[i];
    const uint8_t bSize = byte & 0x03;
    // const bType = byte >> 2 & 0x03;
    // const bTag = bite >> 4;
    const uint8_t size = bSize == 3 ? 4 : bSize;
    const uint8_t type = byte >> 2;
    switch (type) {
    case HID_MAIN_ITEM_TAG_INPUT << 2 | HID_ITEM_TYPE_MAIN:
      if (current.usagePage == HID_USAGE_PAGE_BUTTON) {
        for (size_t j = 0; j < current.reportCount; j++) {
          controls.push_back(Control {
            ControlType::Button,
            current.reportSize,
            current.logicalMin,
            current.logicalMax,
            (uint32_t) current.logicalMin
          });
        }
      } else {
        for (auto usage : current.usages) {
          controls.push_back(Control {
            usage == HID_USAGE_X ? ControlType::Joystick_X :
            usage == HID_USAGE_Y ? ControlType::Joystick_Y :
            usage == HID_USAGE_Z ? ControlType::Joystick_Z :
            usage == HID_USAGE_RZ ? ControlType::Joystick_Rz :
            usage == HID_USAGE_HATSW ? ControlType::HatSwitch :
            ControlType::Noop,
            current.reportSize,
            current.logicalMin,
            current.logicalMax,
            (uint32_t) current.logicalMin
          });
        }
      }
      reportSize += current.reportSize * current.reportCount;
    case HID_MAIN_ITEM_TAG_OUTPUT << 2 | HID_ITEM_TYPE_MAIN:
    case HID_MAIN_ITEM_TAG_COLLECTION << 2 | HID_ITEM_TYPE_MAIN:
    case HID_MAIN_ITEM_TAG_FEATURE << 2 | HID_ITEM_TYPE_MAIN:
    case HID_MAIN_ITEM_TAG_ENDCOLLECTION << 2 | HID_ITEM_TYPE_MAIN:
      current.usages = {};
      break;
    case HID_GLOBAL_ITEM_TAG_REPORT_SIZE << 2 | HID_ITEM_TYPE_GLOBAL:
      current.reportSize = readReportDescriptorData(reportDescriptor, i + 1, size);
      break;
    case HID_GLOBAL_ITEM_TAG_REPORT_COUNT << 2 | HID_ITEM_TYPE_GLOBAL:
      current.reportCount = readReportDescriptorData(reportDescriptor, i + 1, size);
      break;
    case HID_GLOBAL_ITEM_TAG_LOG_MIN << 2 | HID_ITEM_TYPE_GLOBAL:
      current.logicalMin = readReportDescriptorData(reportDescriptor, i + 1, size);
      break;
    case HID_GLOBAL_ITEM_TAG_LOG_MAX << 2 | HID_ITEM_TYPE_GLOBAL:
      current.logicalMax = readReportDescriptorData(reportDescriptor, i + 1, size);
      break;
    case HID_GLOBAL_ITEM_TAG_USAGE_PAGE << 2 | HID_ITEM_TYPE_GLOBAL:
      current.usagePage = readReportDescriptorData(reportDescriptor, i + 1, size);
      break;
    case HID_LOCAL_ITEM_TAG_USAGE << 2 | HID_ITEM_TYPE_LOCAL:
      current.usages.push_back(readReportDescriptorData(reportDescriptor, i + 1, size));
      break;
    default:
      break;
    }
    i += size + 1;
  }
  reportLength = reportSize / 8;
}

uint32_t ReportParser::readReportDescriptorData(
  const uint8_t* reportDescriptor,
  const size_t index,
  const size_t length
) {
  uint32_t data = 0;
  for (size_t i = index + length - 1; i >= index; i--) {
    data = data << 8 | reportDescriptor[i];
  }
  return data;
}
