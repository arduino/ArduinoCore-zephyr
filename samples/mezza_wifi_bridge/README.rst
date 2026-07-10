Overview
********

This sample is a modified version of the zephyr USB CDC-ACM to bridge sample
(look for samples/subsys/usb/cdc_acm_bridge/ in the zephyr).
This samples is intended to be used on Arduino Mezza to flash and control the
radio module from the PC through the USB C UART.

The purpose of this example is to create a bridge between the CDC USB C UART and
the UART connected to the ST67W611M1A6BTR radio module.

The example also control the BOOT pin of the radio module:
- if the BOOT pin is HIGH after reset (transition of the CHIP_EN from LOW to
  HIGH) the radio module goes in UART mode allowing the download of new FW into
  the device
- if the BOOT pin is LOW after reset (transition of the CHIP_EN from LOW to
  HIGH) the radio module goes in FLASH mode allowing the radio module to
  executed the FW previosly downloaded

To put the radio module in UART mode keep the Mezza user button pressed and
reset the board.

In UART mode is possible to download the radio FW, further instructions can be
found here: 
https://arduino.atlassian.net/wiki/spaces/FTHF/pages/6193348624/Mezza+-+WiFi+Module+getting+started

If the board is reset and the USER button is not pressed then the radio module
is in flash mode and it is possible to send commands to control WiFi and
Bluetooth via a minicom session with the device.


