Supported Boards
******************

KIT-PSE84-AI
=============

The `Infineon KIT-PSE84-AI <https://www.infineon.com/evaluation-board/kit-pse84-ai>`_
(PSOC™ Edge E84) is the first supported board for the PSOC™ Edge family.

- FQBN: ``infineon:zephyr_pse84:kit_pse84_ai``
- Silicon: PSE846GPS2DBZC4A

Pinout Diagram
---------------

.. image:: img/KIT_PSE84_AI_Pinout.svg
   :width: 400

.. TODO: add a back-side pinout diagram (with Arduino pin numbers) for the analog pins.
   Once a complete, labeled pinout diagram exists for both sides, consider
   trimming or removing the Macros section below in favor of the diagram.

Macros
------

For the exact macro definitions, refer to
``variants/kit_pse84_ai_pse846gps2dbzc4a_m33/variant.h``. General rules:

- Every exposed GPIO pin from the pinout has a macro identical to the
  documentation name, e.g. ``P17_3``, ``P16_0``, ``P15_3``, etc.
- Digital pins have a macro in the format ``Dx``, where ``x`` is the index of
  the Arduino pin in the range [0, 47], e.g. ``D0``, ``D15``, ``D47``, etc.
- Analog pins have a macro in the format ``Ax``, where ``x`` is the index of
  the Arduino pin in the range [0, 15], e.g. ``A0``, ``A1``, ``A15``, etc.
- Onboard LED and button macros are:

  - ``LED_BUILTIN`` / ``LED_BUILTIN_1``: the primary onboard LED (same pin,
    two names). ``LED_BUILTIN_2`` is the second, distinct onboard LED.
  - ``LED_BUILTIN_ACTIVE``: the logic level that turns an onboard LED on.
  - ``LED_RED`` / ``LED_GREEN`` / ``LED_BLUE``: the individual channels of
    the onboard RGB LED.
  - ``BTN_BUILTIN``: the onboard user button (``SW1``).

- Expansion header pins include the macros as printed on the board:
  ``SERIAL_INTx`` where ``x`` is the index in the range [0, 3], e.g.
  ``SERIAL_INT0``.

.. TODO: document I2C/Wire (SDA, SCL, SDA1, SCL1) and SPI (MOSI, MISO, etc.)
   macros once verified working on this board.
