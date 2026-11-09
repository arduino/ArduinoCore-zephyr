# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# Built into MCUboot only (loader/sysbuild.cmake), when the variant's
# mcuboot.conf turns on the image access hooks.
if(CONFIG_BOOT_IMAGE_ACCESS_HOOKS)
  zephyr_library()
  zephyr_library_sources(boot_hooks.c)
  zephyr_library_link_libraries(MCUBOOT_BOOTUTIL)
endif()
