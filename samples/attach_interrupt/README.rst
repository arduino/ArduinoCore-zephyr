.. _attach_interrupt-sample:

AttachInterrupt
######

Overview
********

This sample demonstrates how to use attachInterrupt API.

Building and Running
********************

Build and flash attachInterrupt sample as follows,

```sh
$> west build -p -S arduino-core -b arduino_nano_33_ble samples/basic/attach_interrupt/

$> west flash --bossac=/home/$USER/.arduino15/packages/arduino/tools/bossac/1.9.1-arduino2/bossac
```

Turn on the LED by detecting interrupts. And Turn off the next interrupt.
