# SPDX-License-Identifier: Apache-2.0

string(REPLACE "__" "_" NORMALIZED_BOARD_TARGET "${NORMALIZED_BOARD_TARGET}")

# Define custom pinctrl state "arduino" for dynamic pin multiplexing.
# Must be set here so both loader and samples see the custom pinctrl state
# while compiling Zephyr/board sources.
zephyr_compile_options(-DPINCTRL_STATE_ARDUINO=PINCTRL_STATE_PRIV_START)

if (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/variants/${BOARD})
  set(variant_dir variants/${BOARD})
elseif (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/variants/${NORMALIZED_BOARD_TARGET})
  set(variant_dir variants/${NORMALIZED_BOARD_TARGET})
else()
if (CONFIG_ARDUINO_API)
  message(FATAL_ERROR "Variant dir not found: variants/${BOARD}, variants/${NORMALIZED_BOARD_TARGET}")
endif()
endif()

if (CONFIG_ARDUINO_API AND NOT CONFIG_LLEXT)
add_subdirectory(cores)
add_subdirectory(libraries)
zephyr_include_directories(${variant_dir})
endif()

