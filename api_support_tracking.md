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
| `setup()` | Core | ✅ | Observed: code runs at all |
| `loop()` | Core | ✅ | Observed: loop cycles confirmed |
| `init()` | Core | ✅ | Observed: millis() works in setup() |
| `initVariant()` | Core | ✅ | Observed: pinMode() works in setup() |
| `millis()` | Timing | ✅ | Working |
| `micros()` | Timing | ✅ | Working |
| `delay()` | Timing | ✅ | Working |
| `yield()` | Timing | ✅ | Working, ~1.5µs overhead confirmed |
| `delayMicroseconds()` | Timing | ⚠️ | Not yet tested |
| `pulseIn()` | Timing | ⚠️ | Not yet tested |
| `pulseInLong()` | Timing | ⚠️ | Not yet tested |
| `pinMode()` | Digital GPIO | ✅ | Working |
| `digitalRead()` | Digital GPIO | ✅ | Working |
| `digitalWrite()` | Digital GPIO | ✅ | Working |
| `digitalPinToInterrupt()` | Digital GPIO | ✅ | Confirmed in interrupt test |
| `shiftIn()` | Digital GPIO | ⚠️ | Not yet tested |
| `shiftOut()` | Digital GPIO | ⚠️ | Not yet tested |
| `tone()` | Digital GPIO | ⚠️ | Not yet tested |
| `noTone()` | Digital GPIO | ⚠️ | Not yet tested |
| `attachInterrupt()` | Interrupts | ✅ | Working, FALLING / RISING / CHANGE confirmed |
| `detachInterrupt()` | Interrupts | ✅ | Working, stops interrupts cleanly |
| `interrupts()` | Interrupts | ⚠️ | Not yet tested |
| `noInterrupts()` | Interrupts | ⚠️ | Not yet tested |
| `analogRead()` | Analog | ❌ | Not supported yet on this PSoC™ Edge port |
| `analogWrite()` | Analog | ❌ | Not supported yet on this PSoC™ Edge port |
| `analogReference()` | Analog | ❌ | Not supported yet on this PSoC™ Edge port |
| `analogReadResolution()` | Analog | ❌ | Not supported yet on this PSoC™ Edge port |
| `analogWriteResolution()` | Analog | ❌ | Not supported yet on this PSoC™ Edge port |
| `Serial.begin()` | Serial / UART | ✅ | Working |
| `Serial.end()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.print()` | Serial / UART | ✅ | Working |
| `Serial.println()` | Serial / UART | ✅ | Working |
| `Serial.write()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.read()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.readBytes()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.readString()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.readStringUntil()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.available()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.availableForWrite()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.peek()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.flush()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.printf()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial.setTimeout()` | Serial / UART | ⚠️ | Not yet tested |
| `Serial1` / `Serial2` | Serial / UART | ⚠️ | Not yet tested |
| USB Serial | Serial / UART | ❌ | Not supported, board uses non-native USB |
| `Wire` / `Wire1` | I2C | ✅ | Supported on this PSoC™ Edge port |
| `Wire.begin()` | I2C | ⚠️ | Not yet tested |
| `Wire.begin(address)` | I2C | ⚠️ | Not yet tested - slave mode |
| `Wire.end()` | I2C | ⚠️ | Not yet tested |
| `Wire.requestFrom()` | I2C | ⚠️ | Not yet tested |
| `Wire.beginTransmission()` | I2C | ⚠️ | Not yet tested |
| `Wire.endTransmission()` | I2C | ⚠️ | Not yet tested |
| `Wire.write()` | I2C | ⚠️ | Not yet tested |
| `Wire.read()` | I2C | ⚠️ | Not yet tested |
| `Wire.available()` | I2C | ⚠️ | Not yet tested |
| `Wire.setClock()` | I2C | ⚠️ | Not yet tested |
| `Wire.onReceive()` | I2C | ⚠️ | Not yet tested - slave mode |
| `Wire.onRequest()` | I2C | ⚠️ | Not yet tested - slave mode |
| `SPI` | SPI | ❌ | Not supported yet on this PSoC™ Edge port |
| `min()` | Math | ⚠️ | Not yet tested |
| `max()` | Math | ⚠️ | Not yet tested |
| `abs()` | Math | ⚠️ | Not yet tested |
| `constrain()` | Math | ⚠️ | Not yet tested |
| `map()` | Math | ⚠️ | Not yet tested |
| `pow()` | Math | ⚠️ | Not yet tested |
| `sqrt()` | Math | ⚠️ | Not yet tested |
| `sin()` | Math | ⚠️ | Not yet tested |
| `cos()` | Math | ⚠️ | Not yet tested |
| `tan()` | Math | ⚠️ | Not yet tested |
| `bit()` | Bits & Bytes | ⚠️ | Not yet tested |
| `bitRead()` | Bits & Bytes | ⚠️ | Not yet tested |
| `bitWrite()` | Bits & Bytes | ⚠️ | Not yet tested |
| `bitSet()` | Bits & Bytes | ⚠️ | Not yet tested |
| `bitClear()` | Bits & Bytes | ⚠️ | Not yet tested |
| `highByte()` | Bits & Bytes | ⚠️ | Not yet tested |
| `lowByte()` | Bits & Bytes | ⚠️ | Not yet tested |
| `word()` | Bits & Bytes | ⚠️ | Not yet tested |
| `random()` | Random | ⚠️ | Not yet tested |
| `randomSeed()` | Random | ⚠️ | Not yet tested |
| `isAlpha()` | Characters | ⚠️ | Not yet tested |
| `isAlphaNumeric()` | Characters | ⚠️ | Not yet tested |
| `isAscii()` | Characters | ⚠️ | Not yet tested |
| `isControl()` | Characters | ⚠️ | Not yet tested |
| `isDigit()` | Characters | ⚠️ | Not yet tested |
| `isGraph()` | Characters | ⚠️ | Not yet tested |
| `isHexadecimalDigit()` | Characters | ⚠️ | Not yet tested |
| `isLowerCase()` | Characters | ⚠️ | Not yet tested |
| `isUpperCase()` | Characters | ⚠️ | Not yet tested |
| `isPrintable()` | Characters | ⚠️ | Not yet tested |
| `isPunct()` | Characters | ⚠️ | Not yet tested |
| `isSpace()` | Characters | ⚠️ | Not yet tested |
| `isWhitespace()` | Characters | ⚠️ | Not yet tested |
| `String()` | String | ⚠️ | Not yet tested |
| `.length()` | String | ⚠️ | Not yet tested |
| `.charAt()` | String | ⚠️ | Not yet tested |
| `.indexOf()` | String | ⚠️ | Not yet tested |
| `.substring()` | String | ⚠️ | Not yet tested |
| `.toUpperCase()` | String | ⚠️ | Not yet tested |
| `.toLowerCase()` | String | ⚠️ | Not yet tested |
| `.toInt()` | String | ⚠️ | Not yet tested |
| `.toFloat()` | String | ⚠️ | Not yet tested |
| `.replace()` | String | ⚠️ | Not yet tested |
| `.trim()` | String | ⚠️ | Not yet tested |
| Threads | Libraries | ❌ | Not supported yet on this PSoC™ Edge port |
| CAN | Libraries | ❌ | Explicitly skipped for this board |
| Ethernet | Libraries | ❌ | Explicitly skipped for this board |
| RTC | Libraries | ❌ | Explicitly skipped for this board |
| WiFi | Libraries | ❌ | Explicitly skipped for this board |

---

#### Reference Links

| Resource | URL |
|----------|-----|
| Arduino API Reference | https://www.arduino.cc/reference/en/ |
| ArduinoCore-API (source of truth) | https://github.com/arduino/ArduinoCore-API |
| ArduinoCore-API `/api` folder | https://github.com/arduino/ArduinoCore-API/tree/master/api |