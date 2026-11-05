.. _serial_event:

Serial Event
############

Overview
********

The serial_event sample echo back serial input data.

Building and Running
********************

Build and flash serial_event sample as follows,

```sh
$> west build -p -S arduino-core -b arduino_nano_33_ble samples/serial_event/

$> west flash --bossac=/home/$USER/.arduino15/packages/arduino/tools/bossac/1.9.1-arduino2/bossac
```
