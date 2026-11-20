# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# Built into MCUboot only (loader/sysbuild.cmake), for the hooks the
# variant's mcuboot.conf turns on.
if(CONFIG_BOOT_IMAGE_ACCESS_HOOKS OR CONFIG_ARDUINO_MCUBOOT_DFU_BOOT_MODE OR
   CONFIG_SOC_FAMILY_MICROCHIP_PIC32CK_SG_GC)
  zephyr_library()
  zephyr_library_sources_ifdef(CONFIG_BOOT_IMAGE_ACCESS_HOOKS boot_hooks.c)
  zephyr_library_sources_ifdef(CONFIG_ARDUINO_MCUBOOT_DFU_BOOT_MODE dfu_boot_mode.c)
  # Same instruction cache setup as the loader.
  zephyr_library_sources_ifdef(CONFIG_SOC_FAMILY_MICROCHIP_PIC32CK_SG_GC ../cmcc_icache.c)
  zephyr_library_link_libraries(MCUBOOT_BOOTUTIL)
endif()
