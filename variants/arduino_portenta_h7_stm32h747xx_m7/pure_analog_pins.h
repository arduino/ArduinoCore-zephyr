#ifndef _PURE_ANALOG_PINS_
#define _PURE_ANALOG_PINS_

/******************************************************************************
 * INCLUDE
 ******************************************************************************/

#include "Arduino.h"

/******************************************************************************
 * PREPROCESSOR-MAGIC
 ******************************************************************************/

#define PURE_ANALOG_AS_DIGITAL_ATTRIBUTE __attribute__ ((error("Can't use pins A0-A3 as digital")))

/******************************************************************************
 * TYPEDEF
 ******************************************************************************/

class PureAnalogPin {
public:
	PureAnalogPin(int _pin) : pin(_pin) {};
	int get() {
		return pin;
	};
	bool operator== (PureAnalogPin const & other) const {
		return pin == other.pin;
	}
	//operator int() = delete;
	__attribute__ ((error("Change me to a #define"))) operator int();
private:
	int pin;
};

extern PureAnalogPin  A0_PURE;
extern PureAnalogPin  A1_PURE;
extern PureAnalogPin  A2_PURE;
extern PureAnalogPin  A3_PURE;

#define A0  A0_PURE
#define A1  A1_PURE
#define A2  A2_PURE
#define A3  A3_PURE

/******************************************************************************
 * FUNCTION DECLARATION
 ******************************************************************************/

void      PURE_ANALOG_AS_DIGITAL_ATTRIBUTE pinMode     (PureAnalogPin pin, PinMode mode);
PinStatus PURE_ANALOG_AS_DIGITAL_ATTRIBUTE digitalRead (PureAnalogPin pin);
void      PURE_ANALOG_AS_DIGITAL_ATTRIBUTE digitalWrite(PureAnalogPin pin, PinStatus value);
int       analogRead  (PureAnalogPin pin);
void      PURE_ANALOG_AS_DIGITAL_ATTRIBUTE analogWrite (PureAnalogPin pin, int value);

#undef PURE_ANALOG_AS_DIGITAL_ATTRIBUTE

#endif /* _PURE_ANALOG_PINS_ */
