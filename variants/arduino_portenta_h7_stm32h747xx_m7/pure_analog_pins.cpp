#include "pure_analog_pins.h"

#undef A0
#undef A1
#undef A2
#undef A3

PureAnalogPin  A0_PURE(0);
PureAnalogPin  A1_PURE(1);
PureAnalogPin  A2_PURE(2);
PureAnalogPin  A3_PURE(3);

int getAnalogReadResolution();

int analogRead(PureAnalogPin pin) {
  return ::analogRead(A0 + pin.get());
}
