# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# Boards with a separate bootloader also get the bootloader updater, uploaded
# by the 'Zephyr burner app' programmer in place of the loader. It is linked where
# the loader lives, so it is only built and never flashed by 'west flash'.
set(bootloader_hex ${CMAKE_CURRENT_LIST_DIR}/../firmwares/${NORMALIZED_BOARD_TARGET}/bootloader.hex)
if(EXISTS ${bootloader_hex})
  ExternalZephyrProject_Add(
    APPLICATION burner
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../burner
    BUILD_ONLY TRUE
  )
endif()
