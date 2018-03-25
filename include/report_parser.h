#ifndef __REPORT_PARSER_H__
#define __REPORT_PARSER_H__

#include <stddef.h>
#include <stdint.h>
#include <vector>

//#include "report_parser_conf.h"

namespace {
  enum struct ControlType {
    Joystick_X,
    Joystick_Y,
    Joystick_Z,
    Joystick_Rz,
    HatSwitch,
    Button,
    Noop
  };
  struct Control {
    const ControlType type;
    const size_t length;
    const int32_t min;
    const int32_t max;
    uint32_t neutral; // int32_t
  };
}

class ReportParser {
  public:
    ReportParser(const uint8_t*, const size_t);
    // struct {
    //   bool buttonsPressing[CONTROL_NUM_MAX];
    //   bool buttonsPressed[CONTROL_NUM_MAX];
    //   size_t buttonsLength = 0;
    //   int8_t axes[CONTROL_NUM_MAX];
    //   size_t axesLength = 0;
    // } status;
    size_t reportLength = 0;
  private:
    bool first = true;
    std::vector<Control> controls;
    static uint32_t readReportDescriptorData(const uint8_t*, const size_t, const size_t);
    static uint32_t readReportData(const uint8_t*, const size_t, const size_t);
};

#endif
