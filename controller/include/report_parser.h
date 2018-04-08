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
    const size_t index;
    const size_t size;
    const int32_t min;
    const int32_t max;
    int32_t neutral;
  };
}

class ReportParser {
  public:
    ReportParser(const uint8_t*, const size_t);
//    struct {int8_t x; int8_t y; int8_t z; int8_t rz;} axes = {0x0f, 0x0f, 0x0f, 0x0f};
    struct {int8_t x; int8_t y; int8_t z; int8_t rz;} axes = {};
    std::vector<bool> buttons;
    uint8_t reportId = 0;
    size_t reportLength;
    void parse(const uint8_t*, const size_t);
  private:
    bool first = true;
    std::vector<Control> controls = {};
    static uint32_t readReportDescriptorData(const uint8_t*, const size_t, const size_t);
    static int32_t readReportData(const uint8_t*, const size_t, const size_t, const bool); // データがint32_tの範囲を超えることはない
    static int32_t castToSigned(const uint32_t, const size_t);
};

#endif
