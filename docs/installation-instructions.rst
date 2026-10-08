Installation Instructions
**************************

You can install the core through the Boards Manager in the Arduino IDE, or via
the Arduino CLI. After installing the core, the loader has to be flashed
before your first sketch can be uploaded.

Integration in Arduino IDE
===========================

Use Arduino IDE 2.x and install the platform through Arduino's Boards Manager.

1. Open Arduino IDE.
2. Navigate to *File > Preferences*.
3. In *Additional boards manager URLs*, add:

   .. code-block:: text

      https://github.com/Infineon/ArduinoCore-zephyr/releases/latest/download/package_infineon_pse84_index.json

4. Open *Boards Manager* (left side menu).
5. Search for *PSOC Edge* and install ``Infineon PSOC Edge Boards``.
6. Select the board and the correct serial port:
   *Tools > Board > Infineon PSOC Edge Boards > Infineon KIT-PSE84-AI (PSOC Edge E84)*
   and *Tools > Port*.
7. Flash the loader: *Tools > Burn Bootloader*.
8. Open or write a sketch and click *Upload*.

Installation in Arduino CLI
=============================

Install the core:

.. code-block:: bash

   arduino-cli core install infineon:zephyr_pse84 --additional-urls https://github.com/Infineon/ArduinoCore-zephyr/releases/latest/download/package_infineon_pse84_index.json

Confirm the core installed correctly:

.. code-block:: bash

   arduino-cli core list

The FQBN for the Infineon KIT-PSE84-AI (PSOC Edge E84) is
``infineon:zephyr_pse84:kit_pse84_ai``.

Flash the loader:

.. code-block:: bash

   arduino-cli burn-bootloader -b infineon:zephyr_pse84:kit_pse84_ai

Compile and upload a sketch:

.. code-block:: bash

   arduino-cli compile -b infineon:zephyr_pse84:kit_pse84_ai MySketch
   arduino-cli upload -b infineon:zephyr_pse84:kit_pse84_ai -p <port> MySketch
