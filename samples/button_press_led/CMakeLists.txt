# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# get value of NORMALIZED_BOARD_TARGET early
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE} COMPONENTS yaml boards)

if (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../variants/${NORMALIZED_BOARD_TARGET}_${BOARD_REVISION})
  set(variant_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../variants/${NORMALIZED_BOARD_TARGET}_${BOARD_REVISION})
elseif (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../variants/${NORMALIZED_BOARD_TARGET})
  set(variant_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../variants/${NORMALIZED_BOARD_TARGET})
elseif (IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../variants/${BOARD})
  set(variant_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../variants/${BOARD})
endif()

if (EXISTS ${variant_dir}/${NORMALIZED_BOARD_TARGET}_${BOARD_REVISION}.overlay)
  set(DTC_OVERLAY_FILE ${variant_dir}/${NORMALIZED_BOARD_TARGET}_${BOARD_REVISION}.overlay)
elseif (EXISTS ${variant_dir}/${NORMALIZED_BOARD_TARGET}.overlay)
  set(DTC_OVERLAY_FILE ${variant_dir}/${NORMALIZED_BOARD_TARGET}.overlay)
elseif (EXISTS ${variant_dir}/${BOARD}.overlay)
  set(DTC_OVERLAY_FILE ${variant_dir}/${BOARD}.overlay)
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(button_press_led)

target_sources(app PRIVATE src/main.cpp)

zephyr_compile_options(-Wno-unused-variable -Wno-comment)
