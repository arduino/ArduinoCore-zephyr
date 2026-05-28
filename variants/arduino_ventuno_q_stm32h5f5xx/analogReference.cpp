#include <Arduino.h>

extern "C" {
    int analog_reference(uint8_t mode);
};

void analogReference(uint8_t mode)
{
  analog_reference(mode);
}