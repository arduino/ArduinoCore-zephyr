.. _fade:

Fade
####

Overview
********

The Fade sample gradually increases/decreases the voltage of the output pin.
When connecting the LED to the output pin, the LED blinks gradually.

Building and Running
********************

Build and flash Fade sample as follows,

```sh
$> west build -p -S arduino-core -b arduino_nano_33_ble samples/basic/fade/

$> west flash --bossac=/home/$USER/.arduino15/packages/arduino/tools/bossac/1.9.1-arduino2/bossac
```
