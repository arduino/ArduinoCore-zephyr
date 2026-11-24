# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# A variant enables MCUboot in its sysbuild.conf. Its configuration lives in
# mcuboot/, like the burner in burner/: prj.conf for every board, plus the
# board's own boards/<board>.conf and .overlay. Sysbuild would only look for
# them in loader/sysbuild/mcuboot, so point the MCUboot image to mcuboot/.
if(SB_CONFIG_BOOTLOADER_MCUBOOT)
  set(mcuboot_APPLICATION_CONFIG_DIR ${CMAKE_CURRENT_LIST_DIR}/../mcuboot
    CACHE INTERNAL "MCUboot configuration directory")
  # the MCUboot snippets are in mcuboot/snippets
  set(mcuboot_SNIPPET_ROOT ${CMAKE_CURRENT_LIST_DIR}/../mcuboot
    CACHE INTERNAL "MCUboot snippet root")
  # serial recovery replies must not be truncated, see mcuboot/snippets: add
  # the snippet to any given with -Dmcuboot_SNIPPET (sysbuild only passes cache
  # variables to the images)
  set(mcuboot_snippets $CACHE{mcuboot_SNIPPET})
  if(NOT mcuboot-cdc-flow-control IN_LIST mcuboot_snippets)
    list(APPEND mcuboot_snippets mcuboot-cdc-flow-control)
    set(mcuboot_SNIPPET "${mcuboot_snippets}" CACHE STRING "MCUboot snippets" FORCE)
  endif()

  # configured shields are for the loader only
  set(mcuboot_SHIELD "" CACHE STRING "MCUboot shields" FORCE)
endif()

# Boards with a separate bootloader also get the bootloader updater, uploaded
# by the 'Zephyr burner app' programmer in place of the loader. It is linked where
# the loader lives, so it is only built and never flashed by 'west flash'.
# The bootloader is the MCUboot image built here, if any, or else the prebuilt
# firmwares/<variant>/bootloader.hex (see burner/CMakeLists.txt).
set(bootloader_hex ${CMAKE_CURRENT_LIST_DIR}/../firmwares/${NORMALIZED_BOARD_TARGET}/bootloader.hex)
if(SB_CONFIG_BOOTLOADER_MCUBOOT OR EXISTS ${bootloader_hex})
  ExternalZephyrProject_Add(
    APPLICATION burner
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../burner
    BUILD_ONLY TRUE
  )
  # same version as the loader (and the core), see extra/build.sh
  set(burner_APP_BUILD_VERSION ${APP_BUILD_VERSION} CACHE INTERNAL "burner build version")
endif()
if(SB_CONFIG_BOOTLOADER_MCUBOOT)
  # MCUboot is built in its own image directory, before the burner embeds it
  set(burner_BOOTLOADER_MCUBOOT_DIR ${CMAKE_BINARY_DIR}/mcuboot CACHE INTERNAL "MCUboot build directory")
  add_dependencies(burner mcuboot)
  # MCUboot only boots signed images in the loader slot: sign the burner like
  # the loader, applying the main image defaults (MCUboot mode, signing key)
  set_property(TARGET burner APPEND PROPERTY IMAGE_CONF_SCRIPT
    ${ZEPHYR_BASE}/share/sysbuild/image_configurations/MAIN_image_default.cmake)
endif()
