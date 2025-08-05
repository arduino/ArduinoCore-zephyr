#include "WiFi.h"

WiFiClass WiFi;

String WiFiClass::firmwareVersion() { // TODO integrate fw version detection
#if defined(ARDUINO_PORTENTA_C33)
    return "v1.5.0";
#elif defined(ARDUINO_PORTENTA_H7) || defined(ARDUINO_OPTA) || defined(ARDUINO_GIGA) ||\
    defined(ARDUINO_NICLA_VISION) || defined(ARDUINO_NICLA_SENSE_ME)
    return "v0.0.0";
#else
    return "v0.0.0";
#endif
}
