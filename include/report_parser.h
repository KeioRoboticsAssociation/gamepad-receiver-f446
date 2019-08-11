#ifndef REPORT_PARSER_H_
#define REPORT_PARSER_H_

#include <inttypes.h>
#include <stddef.h>
#include <vector>

class ReportParser {
  public:
    ReportParser(const uint8_t*, const size_t);
    void parse(const uint8_t*);
    struct {int8_t x; int8_t y; int8_t z; int8_t rz;} axes = {};
    std::vector<bool> buttons;
    uint8_t reportId = 0;
    size_t reportLength;
  private:
    enum struct ControlType {
      JOYSTICK_X,
      JOYSTICK_Y,
      JOYSTICK_Z,
      JOYSTICK_Rz,
      HATSWITCH,
      BUTTON,
      NOOP,
    };
    struct Control {
      const ControlType type;
      const size_t index;
      const size_t size;
      const int32_t min;
      const int32_t max;
      int32_t neutral;
    };
    static uint32_t readReportDescriptorData(const uint8_t*, const size_t, const size_t);
    static int32_t readReportData(const uint8_t*, const size_t, const size_t, const bool); // データがint32_tの範囲を超えることはない
    static int32_t castToSigned(const uint32_t, const size_t);
    bool first = true;
    std::vector<Control> controls;
};

#endif
