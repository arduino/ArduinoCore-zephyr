### PSE84 Arduino API Support Tracking

#### Legend
| Symbol | Meaning |
|--------|---------|
| ✅ | Confirmed working |
| ❌ | Confirmed not supported |
| ⚠️ | Not yet tested |

---

#### API Support Table

| Function | Scope | Status | Notes |
|----------|-------|--------|-------|
| **Digital I/O** |  |  |  |
| `digitalRead()` | Digital GPIO | ✅ | Working |
| `digitalWrite()` | Digital GPIO | ✅ | Working |
| `pinMode()` | Digital GPIO | ✅ | Working |
| **Analog I/O** |  |  |  |
| `analogRead()` | Analog | ❌ | Not supported yet on this PSOC™ Edge port |
| `analogReadResolution()` | Analog | ❌ | Not supported yet on this PSOC™ Edge port |
| `analogReference()` | Analog | ❌ | Not supported yet on this PSOC™ Edge port |
| `analogWrite()` | Analog | ❌ | Not supported yet on this PSOC™ Edge port |
| `analogWriteResolution()` | Analog | ❌ | Not supported yet on this PSOC™ Edge port |
| **Advanced I/O** |  |  |  |
| `tone()` | Digital GPIO | ⚠️ | Not yet tested |
| `noTone()` | Digital GPIO | ⚠️ | Not yet tested |
| `pulseIn()` | Digital GPIO | ⚠️ | Not yet tested |
| `pulseInLong()` | Digital GPIO | ⚠️ | Not yet tested |
| `shiftIn()` | Digital GPIO | ⚠️ | Not yet tested |
| `shiftOut()` | Digital GPIO | ⚠️ | Not yet tested |
| **Time** |  |  |  |
| `delay()` | Timing | ✅ | Working |
| `delayMicroseconds()` | Timing | ⚠️ | Not yet tested |
| `millis()` | Timing | ✅ | Working |
| `micros()` | Timing | ✅ | Working |
| **Math** |  |  |  |
| `abs()` | Math | ⚠️ | Not yet tested |
| `constrain()` | Math | ⚠️ | Not yet tested |
| `map()` | Math | ⚠️ | Not yet tested |
| `max()` | Math | ⚠️ | Not yet tested |
| `min()` | Math | ⚠️ | Not yet tested |
| `pow()` | Math | ⚠️ | Not yet tested |
| `sq()` | Math | ⚠️ | Not yet tested |
| `sqrt()` | Math | ⚠️ | Not yet tested |
| **Trigonometry** |  |  |  |
| `cos()` | Math | ⚠️ | Not yet tested |
| `sin()` | Math | ⚠️ | Not yet tested |
| `tan()` | Math | ⚠️ | Not yet tested |
| **Characters** |  |  |  |
| `isAlpha()` | Characters | ⚠️ | Not yet tested |
| `isAlphaNumeric()` | Characters | ⚠️ | Not yet tested |
| `isAscii()` | Characters | ⚠️ | Not yet tested |
| `isControl()` | Characters | ⚠️ | Not yet tested |
| `isDigit()` | Characters | ⚠️ | Not yet tested |
| `isGraph()` | Characters | ⚠️ | Not yet tested |
| `isHexadecimalDigit()` | Characters | ⚠️ | Not yet tested |
| `isLowerCase()` | Characters | ⚠️ | Not yet tested |
| `isPrintable()` | Characters | ⚠️ | Not yet tested |
| `isPunct()` | Characters | ⚠️ | Not yet tested |
| `isSpace()` | Characters | ⚠️ | Not yet tested |
| `isUpperCase()` | Characters | ⚠️ | Not yet tested |
| `isWhitespace()` | Characters | ⚠️ | Not yet tested |
| **Random Numbers** |  |  |  |
| `random()` | Random | ⚠️ | Not yet tested |
| `randomSeed()` | Random | ⚠️ | Not yet tested |
| **Bits and Bytes** |  |  |  |
| `bit()` | Bits & Bytes | ⚠️ | Not yet tested |
| `bitClear()` | Bits & Bytes | ⚠️ | Not yet tested |
| `bitRead()` | Bits & Bytes | ⚠️ | Not yet tested |
| `bitSet()` | Bits & Bytes | ⚠️ | Not yet tested |
| `bitWrite()` | Bits & Bytes | ⚠️ | Not yet tested |
| `highByte()` | Bits & Bytes | ⚠️ | Not yet tested |
| `lowByte()` | Bits & Bytes | ⚠️ | Not yet tested |
| **External Interrupts** |  |  |  |
| `attachInterrupt()` | Interrupts | ✅ | Working, FALLING / RISING / CHANGE confirmed |
| `detachInterrupt()` | Interrupts | ✅ | Working, stops interrupts cleanly |
| `digitalPinToInterrupt()` | Interrupts | ✅ | Confirmed in interrupt test |
| **Interrupts** |  |  |  |
| `interrupts()` | Interrupts | ⚠️ | Not yet tested |
| `noInterrupts()` | Interrupts | ⚠️ | Not yet tested |
| **Stream** |  |  |  |
| `available()` | Serial / UART | ⚠️ | Not yet tested |
| `read()` | Serial / UART | ⚠️ | Not yet tested |
| `flush()` | Serial / UART | ⚠️ | Not yet tested |
| `find()` | Serial / UART | ⚠️ | Not yet tested |
| `findUntil()` | Serial / UART | ⚠️ | Not yet tested |
| `peek()` | Serial / UART | ⚠️ | Not yet tested |
| `readBytes()` | Serial / UART | ⚠️ | Not yet tested |
| `readBytesUntil()` | Serial / UART | ⚠️ | Not yet tested |
| `readString()` | Serial / UART | ⚠️ | Not yet tested |
| `readStringUntil()` | Serial / UART | ⚠️ | Not yet tested |
| `parseInt()` | Serial / UART | ⚠️ | Not yet tested |
| `parseFloat()` | Serial / UART | ⚠️ | Not yet tested |
| `setTimeout()` | Serial / UART | ⚠️ | Not yet tested |
| **Serial** |  |  |  |
| `if(Serial)` | Serial / UART | ⚠️ | Not yet tested |
| `Serial1` / `Serial2` | Serial / UART | ⚠️ | Not yet tested |
| USB Serial | Serial / UART | ⚠️ | Not yet tested |
| `Serial.available()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.availableForWrite()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.begin()` | Serial / UART | ✅ | Working |
| `Serial.end()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.find()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.findUntil()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.flush()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.parseFloat()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.parseInt()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.peek()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.print()` | Serial / UART | ✅ | Working |
| `Serial.println()` | Serial / UART | ✅ | Working |
| `Serial.read()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.readBytes()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.readBytesUntil()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.readString()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.readStringUntil()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.setTimeout()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.write()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.serialEvent()` | Serial / UART | ⚠️ | Not yet tested |
| **SPI** |  |  |  |
| `SPI` | SPI | ❌ | Not supported yet on this PSOC™ Edge port |
| I2C (Wire) |  |  |  |
| `Wire` / `Wire1` | I2C | ✅ | Supported on this PSOC™ Edge port |
| `Wire.begin()` | I2C | ⚠️ | Not yet tested |
| `Wire.end()` | I2C | ⚠️ | Not yet tested |
| `Wire.requestFrom()` | I2C | ⚠️ | Not yet tested |
| `Wire.beginTransmission()` | I2C | ⚠️ | Not yet tested |
| `Wire.endTransmission()` | I2C | ⚠️ | Not yet tested |
| `Wire.write()` | I2C | ⚠️ | Not yet tested |
| `Wire.available()` | I2C | ⚠️ | Not yet tested |
| `Wire.read()` | I2C | ⚠️ | Not yet tested |
| `Wire.setClock()` | I2C | ⚠️ | Not yet tested |
| `Wire.onReceive()` | I2C | ⚠️ | Not yet tested - slave mode |
| `Wire.onRequest()` | I2C | ⚠️ | Not yet tested - slave mode |
| `Wire.setWireTimeout()` | I2C | ⚠️ | Not yet tested |
| `Wire.clearWireTimeoutFlag()` | I2C | ⚠️ | Not yet tested |
| `Wire.getWireTimeoutFlag()` | I2C | ⚠️ | Not yet tested |
| **Sketch** |  |  |  |
| `loop()` | Core | ✅ | Observed: loop cycles confirmed |
| `setup()` | Core | ✅ | Observed: code runs at all |
| **Other** |  |  |  |
| `init()` | Core | ⚠️ | Observed: millis() works in setup() |
| `initVariant()` | Core | ⚠️ | Observed: pinMode() works in setup() |
| `yield()` | Timing | ✅ | Working, ~1.5µs overhead confirmed |
| Threads | Libraries | ❌ | Not supported yet on this PSOC™ Edge port |
| CAN | Libraries | ❌ | Not supported yet on this PSOC™ Edge port |
| Ethernet | Libraries | ❌ | Not supported yet on this PSOC™ Edge port |
| RTC | Libraries | ❌ | Not supported yet on this PSOC™ Edge port |
| WiFi | Libraries | ❌ | Not supported yet on this PSOC™ Edge port |
| USB Peripherals | Libraries | ❌ | Not supported yet on this PSOC™ Edge port |

---

#### Reference Links

| Resource | URL |
|----------|-----|
| Arduino API Reference | https://www.arduino.cc/reference/en/ |
| Arduino API Programming Reference | https://docs.arduino.cc/learn/programming/reference/ |
| ArduinoCore-API (source of truth) | https://github.com/arduino/ArduinoCore-API |
| ArduinoCore-API `/api` folder | https://github.com/arduino/ArduinoCore-API/tree/master/api |