# SPDX-License-Identifier: Apache-2.0

# Do nothing unless the Arduino API is enabled in the Zephyr project
if(NOT CONFIG_ARDUINO_API)
  return()
endif()

# Keep this in sync with zephyr/module.yml
set(ARDUINO_API_BLOB_VERSION 1.5.2)

string(REPLACE "__" "_" NORMALIZED_BOARD_TARGET "${NORMALIZED_BOARD_TARGET}")

# Define custom pinctrl states "arduino" and "arduino_alt" for dynamic pin multiplexing.
# Must be set here so both loader and samples see the custom pinctrl states
# while compiling Zephyr/board sources.
zephyr_compile_options(
  -DPINCTRL_STATE_ARDUINO=PINCTRL_STATE_PRIV_START
  -DPINCTRL_STATE_ARDUINO_ALT=PINCTRL_STATE_PRIV_START+1
)

if (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/variants/${BOARD})
  set(variant_dir variants/${BOARD})
elseif (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/variants/${NORMALIZED_BOARD_TARGET})
  set(variant_dir variants/${NORMALIZED_BOARD_TARGET})
else()
  message(FATAL_ERROR "Variant dir not found: variants/${BOARD}, variants/${NORMALIZED_BOARD_TARGET}")
endif()

# API archives embed an 'ArduinoCore-API-<version>' directory
set(ARDUINO_API_BLOB_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/zephyr/blobs/ArduinoCore-API/)
set(ARDUINO_API_BLOB_DIR     ${ARDUINO_API_BLOB_ROOT_DIR}/ArduinoCore-API-${ARDUINO_API_BLOB_VERSION})
set(ARDUINO_API_BLOB_ARCHIVE ${ARDUINO_API_BLOB_DIR}.tar.gz)

if(NOT EXISTS ${ARDUINO_API_BLOB_ARCHIVE})
  message(FATAL_ERROR "Arduino API ${ARDUINO_API_BLOB_VERSION} must be downloaded with 'west blobs fetch'.")
endif()

if(NOT IS_DIRECTORY ${ARDUINO_API_BLOB_DIR})
  # A new version of the API was downloaded, so extract it to the blob
  # directory and copy it to cores/arduino/api
  file(ARCHIVE_EXTRACT
    INPUT ${ARDUINO_API_BLOB_ARCHIVE}
    DESTINATION ${ARDUINO_API_BLOB_ROOT_DIR}
    PATTERNS */api/* # only extract the 'api' directory from the archive
  )
  set(update_api_copy TRUE)
elseif(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cores/arduino/api)
  # There's nothing where the API should be, copy the extracted API files
  set(update_api_copy TRUE)
else()
  set(update_api_copy FALSE)
endif()

if(update_api_copy)
  file(REMOVE_RECURSE ${CMAKE_CURRENT_SOURCE_DIR}/cores/arduino/api)
  file(COPY ${ARDUINO_API_BLOB_DIR}/api DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/cores/arduino)
endif()

if (NOT CONFIG_LLEXT_EDK)
  add_subdirectory(cores)
  add_subdirectory(libraries)
  zephyr_include_directories(${variant_dir})

  # It's fine to use GLOB here because the API files will only change when
  # ARDUINO_API_BLOB_VERSION is modified above, triggering a re-run of CMake.
  file(GLOB ARDUINO_API_SOURCES "cores/arduino/api/*.cpp")
  zephyr_sources(${ARDUINO_API_SOURCES})
endif()
