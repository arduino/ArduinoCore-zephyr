Environment Setup
*******************

.. note::
   This setup is currently documented for Linux and WSL environments.

Prerequisites
==============

- Python 3.12 or newer, with ``pip`` and ``venv``.
- On Ubuntu or other apt-based distributions:

  .. code-block:: bash

     sudo apt install python3-pip python3-setuptools python3-venv \
       build-essential git cmake ninja-build zstd jq rsync

Clone the repository
======================

.. code-block:: bash

   git clone https://github.com/Infineon/ArduinoCore-zephyr.git
   cd ArduinoCore-zephyr

Run the bootstrap script
==========================

.. code-block:: bash

   ./extra/bootstrap.sh

This downloads Zephyr and the required HALs, installs the Arm toolchain, and
fetches the required firmware blobs. Allow approximately 15-30 minutes on the
first run.

Build and flash the loader
=============================

.. code-block:: bash

   ./extra/build.sh kit_pse84_ai
   west flash -d build/kit_pse84_ai_pse846gps2dbzc4a_m33

Flashing requires a KitProg3-compatible OpenOCD with the board's interface
script. If you hit ``Can't find interface/kitprog3.cfg`` or similar errors,
install `Infineon OpenOCD <https://github.com/Infineon/openocd/releases>`_
(minimum v5.8.0) and point west at it in ``~/.west/config``:

.. code-block:: ini

   [runner.openocd]
   openocd = /opt/openocd/bin/openocd
   openocd_search = /opt/openocd/scripts

Install the Arduino CLI
==========================

.. code-block:: bash

   mkdir -p ~/.local/bin
   curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/.local/bin sh
   echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
   source ~/.bashrc

Install the core and tools
=============================

.. code-block:: bash

   arduino-cli core install infineon:zephyr_pse84 \
     --additional-urls https://github.com/Infineon/ArduinoCore-zephyr/releases/latest/download/package_infineon_pse84_index.json

Local development
====================

To compile against your local checkout instead of the released core,
symlink it into the sketchbook's hardware folder:

.. code-block:: bash

   mkdir -p ~/Arduino/hardware/infineon
   ln -s ~/ArduinoCore-zephyr ~/Arduino/hardware/infineon/zephyr_pse84

The sketchbook hardware folder takes precedence over the installed package
for the same FQBN. Remove the symlink to revert to the released core.

Compile and upload
======================

Uploading requires permission to open the serial port:

.. code-block:: bash

   sudo usermod -aG dialout "$USER"

.. code-block:: bash

   arduino-cli sketch new MySketch
   arduino-cli compile -b infineon:zephyr_pse84:kit_pse84_ai MySketch
   arduino-cli upload -b infineon:zephyr_pse84:kit_pse84_ai -p /dev/ttyACM0 MySketch
