### PSE84 Arduino API Support Tracking

#### Legend
| Symbol | Meaning |
|--------|---------|
| ✅ | Confirmed working |
| ⚠️ | Not yet tested |
| ❌ | Not yet supported |

---

#### API Support Table

| Function | Scope | Status |
|----------|-------|--------|
| **Digital I/O** |  |  |
| `digitalRead()` | Digital GPIO | ✅ |
| `digitalWrite()` | Digital GPIO | ✅ |
| `pinMode()` | Digital GPIO | ✅ |
| **Analog I/O** |  |  |
| `analogRead()` | Analog | ❌ |
| `analogReadResolution()` | Analog | ❌ |
| `analogReference()` | Analog | ❌ |
| `analogWrite()` | Analog | ❌ |
| `analogWriteResolution()` | Analog | ❌ |
| **Advanced I/O** |  |  |
| `tone()` | Digital GPIO | ✅ |
| `noTone()` | Digital GPIO | ✅ |
| `pulseIn()` | Digital GPIO | ✅ |
| `pulseInLong()` | Digital GPIO | ❌ |
| `shiftIn()` | Digital GPIO | ✅ |
| `shiftOut()` | Digital GPIO | ✅ |
| **Time** |  |  |
| `delay()` | Timing | ✅ |
| `delayMicroseconds()` | Timing | ✅ |
| `millis()` | Timing | ✅ |
| `micros()` | Timing | ✅ |
| **Math** |  |  |
| `abs()` | Math | ✅ |
| `constrain()` | Math | ✅ |
| `map()` | Math | ✅ |
| `max()` | Math | ✅ |
| `min()` | Math | ✅ |
| `pow()` | Math | ✅ |
| `sq()` | Math | ✅ |
| `sqrt()` | Math | ✅ |
| **Trigonometry** |  |  |
| `cos()` | Math | ✅ |
| `sin()` | Math | ✅ |
| `tan()` | Math | ✅ |
| **Characters** |  |  |
| `isAlpha()` | Characters | ✅ |
| `isAlphaNumeric()` | Characters | ✅ |
| `isAscii()` | Characters | ✅ |
| `isControl()` | Characters | ✅ |
| `isDigit()` | Characters | ✅ |
| `isGraph()` | Characters | ✅ |
| `isHexadecimalDigit()` | Characters | ✅ |
| `isLowerCase()` | Characters | ✅ |
| `isPrintable()` | Characters | ✅ |
| `isPunct()` | Characters | ✅ |
| `isSpace()` | Characters | ✅ |
| `isUpperCase()` | Characters | ✅ |
| `isWhitespace()` | Characters | ✅ |
| **Random Numbers** |  |  |
| `random()` | Random | ✅ |
| `randomSeed()` | Random | ✅ |
| **Bits and Bytes** |  |  |
| `bit()` | Bits & Bytes | ✅ |
| `bitClear()` | Bits & Bytes | ✅ |
| `bitRead()` | Bits & Bytes | ✅ |
| `bitSet()` | Bits & Bytes | ✅ |
| `bitWrite()` | Bits & Bytes | ✅ |
| `highByte()` | Bits & Bytes | ✅ |
| `lowByte()` | Bits & Bytes | ✅ |
| **External Interrupts** |  |  |
| `attachInterrupt()` | Interrupts | ✅ |
| `detachInterrupt()` | Interrupts | ✅ |
| `digitalPinToInterrupt()` | Interrupts | ✅ |
| **Interrupts** |  |  |
| `interrupts()` | Interrupts | ✅ |
| `noInterrupts()` | Interrupts | ✅ |
| **Stream** |  |  |
| `available()` | Serial / UART | ✅ |
| `read()` | Serial / UART | ✅ |
| `flush()` | Serial / UART | ✅ _(works on Serial; hangs on Serial1)_ |
| `find()` | Serial / UART | ✅ |
| `findUntil()` | Serial / UART | ✅ |
| `peek()` | Serial / UART | ✅ |
| `readBytes()` | Serial / UART | ✅ |
| `readBytesUntil()` | Serial / UART | ✅ |
| `readString()` | Serial / UART | ✅ |
| `readStringUntil()` | Serial / UART | ✅ |
| `parseInt()` | Serial / UART | ✅ |
| `parseFloat()` | Serial / UART | ✅ |
| `setTimeout()` | Serial / UART | ✅ |
| **Serial** |  |  |
| `if(Serial)` | Serial / UART | ✅ |
| `Serial1` / `Serial2` | Serial / UART | ⚠️ |
| USB Serial | Serial / UART | ⚠️ |
| `Serial.available()` | Serial / UART | ✅ |
| `Serial.availableForWrite()` | Serial / UART | ✅ |
| `Serial.begin()` | Serial / UART | ✅ |
| `Serial.end()` | Serial / UART | ✅ |
| `Serial.find()` | Serial / UART | ✅ |
| `Serial.findUntil()` | Serial / UART | ✅ |
| `Serial.flush()` | Serial / UART | ✅ |
| `Serial.parseFloat()` | Serial / UART | ✅ |
| `Serial.parseInt()` | Serial / UART | ✅ |
| `Serial.peek()` | Serial / UART | ✅ |
| `Serial.print()` | Serial / UART | ✅ |
| `Serial.println()` | Serial / UART | ✅ |
| `Serial.read()` | Serial / UART | ✅ |
| `Serial.readBytes()` | Serial / UART | ✅ |
| `Serial.readBytesUntil()` | Serial / UART | ✅ |
| `Serial.readString()` | Serial / UART | ✅ |
| `Serial.readStringUntil()` | Serial / UART | ✅ |
| `Serial.setTimeout()` | Serial / UART | ✅ |
| `Serial.write()` | Serial / UART | ✅ |
| `Serial.serialEvent()` | Serial / UART | ⚠️ |
| **SPI** |  |  |
| `SPI` | SPI | ❌ |
| I2C (Wire) |  |  |
| `Wire` / `Wire1` | I2C | ✅ |
| `Wire.begin()` | I2C | ⚠️ |
| `Wire.end()` | I2C | ⚠️ |
| `Wire.requestFrom()` | I2C | ⚠️ |
| `Wire.beginTransmission()` | I2C | ⚠️ |
| `Wire.endTransmission()` | I2C | ⚠️ |
| `Wire.write()` | I2C | ⚠️ |
| `Wire.available()` | I2C | ⚠️ |
| `Wire.read()` | I2C | ⚠️ |
| `Wire.setClock()` | I2C | ⚠️ |
| `Wire.onReceive()` | I2C | ⚠️ |
| `Wire.onRequest()` | I2C | ⚠️ |
| `Wire.setWireTimeout()` | I2C | ⚠️ |
| `Wire.clearWireTimeoutFlag()` | I2C | ⚠️ |
| `Wire.getWireTimeoutFlag()` | I2C | ⚠️ |
| **Sketch** |  |  |
| `loop()` | Core | ✅ |
| `setup()` | Core | ✅ |
| **Other** |  |  |
| `init()` | Core | ❌ |
| `initVariant()` | Core | ✅ |
| `yield()` | Timing | ✅ |
| Threads | Libraries | ❌ |
| CAN | Libraries | ❌ |
| Ethernet | Libraries | ❌ |
| RTC | Libraries | ❌ |
| WiFi | Libraries | ❌ |
| USB Peripherals | Libraries | ❌ |

---

#### Reference Links

| Resource | URL |
|----------|-----|
| Arduino API Reference | https://www.arduino.cc/reference/en/ |
| Arduino API Programming Reference | https://docs.arduino.cc/learn/programming/reference/ |
| ArduinoCore-API (source of truth) | https://github.com/arduino/ArduinoCore-API |
| ArduinoCore-API `/api` folder | https://github.com/arduino/ArduinoCore-API/tree/master/api |

---

#### Arduino Core Unity Test Results

| Test | Category | Status |
|------|----------|--------|
| **Digital I/O** |  |  |
| `test_digitalio_single` | Digital I/O | ⚠️ |
| **Analog I/O** |  |  |
| `test_analogio_adc` | Analog ADC | ❌ COMPILE FAIL |
| `test_analogio_pwm` | Analog PWM | ❌ COMPILE FAIL |
| `test_analogio_dac` | Analog DAC | ❌ COMPILE FAIL |
| **Advanced I/O** |  |  |
| `test_tone_no_tone` | Tone | ❌ COMPILE FAIL |
| `test_pulse_board1` | Pulse | ⚠️ |
| `test_pulse_board2` | Pulse | ⚠️ |
| **Time** |  |  |
| `test_time_single` | Time | ✅ |
| **Random Numbers** |  |  |
| `test_random` | Random | ✅ |
| **External Interrupts** |  |  |
| `test_interrupts_single` | Interrupts | ✅ |
| **Serial (UART)** |  |  |
| `test_uart_tx` | UART | ⚠️ |
| `test_uart_rx` | UART | ⚠️ |
| `test_uart_connected2_tx` | UART | ⚠️ |
| `test_uart_connected2_rx` | UART | ⚠️ |
| **SPI** |  |  |
| `test_spi_connected1_loopback` | SPI | ⚠️ |
| `test_spi_connected2_masterpingpong` | SPI | 🟢 |
| `test_spi_connected2_slavepingpong` | SPI | 🟢 |
| **I2C (Wire)** |  |  |
| `test_wire_connected1_pingpong` | I2C/Wire | ❌ |
| `test_wire_connected2_masterpingpong` | I2C/Wire | 🟢 |
| `test_wire_connected2_slavepingpong` | I2C/Wire | 🟢 |
| **CAN** |  |  |
| `test_can_single` | CAN | ❌ COMPILE FAIL |
| `test_can_connected2_node1` | CAN | ❌ COMPILE FAIL |
| `test_can_connected2_node2` | CAN | ❌ COMPILE FAIL |
| **OneWire** |  |  |
| `test_onewire_DS18x20` | OneWire | 🟢 |
| **WiFi** |  |  |
| `test_wifi_sta` | WiFi | ❌ COMPILE FAIL |
| `test_wifi_ap` | WiFi | ❌ COMPILE FAIL |
| `test_wifi_client` | WiFi | ❌ COMPILE FAIL |
| `test_wifi_server` | WiFi | ❌ COMPILE FAIL |
| `test_wifi_udp_client` | WiFi | ❌ COMPILE FAIL |
| `test_wifi_udp_server` | WiFi | ❌ COMPILE FAIL |
| `test_wifi_extras` | WiFi | ❌ COMPILE FAIL |
| `test_wifi_exceptions` | WiFi | ❌ COMPILE FAIL |

| Status | Meaning |
|--------|---------|
| ✅ PASS | All checks passed on hardware |
| ⚠️ PARTIAL | Ran on hardware; some checks failed or did not complete |
| 🟢 NOT RUN | Compiles, but not yet run on hardware (missing setup or hardware) |
| ❌ FAIL | Ran on hardware and failed |
| ❌ COMPILE FAIL | Does not compile |
