# SPDX-License-Identifier: Apache-2.0
zephyr_include_directories(.)
zephyr_include_directories(../../variants)

if(NOT DEFINED ARDUINO_BUILD_PATH)

zephyr_sources(zephyrSerial.cpp)
zephyr_sources(zephyrCommon.cpp)
zephyr_sources(zephyrPinctrl.cpp)
zephyr_sources(USB.cpp)
zephyr_sources(itoa.cpp)

zephyr_sources(wiring_analog.cpp)
zephyr_sources(wiring_digital.cpp)
zephyr_sources(wiring_pulse.cpp)
zephyr_sources(Tone.cpp)
zephyr_sources(WMath.cpp)
zephyr_sources(WInterrupts.cpp)

if(DEFINED CONFIG_ARDUINO_ENTRY)
zephyr_sources(main.cpp)
zephyr_sources(threads.cpp)
endif()

endif()

