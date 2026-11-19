# SPDX-License-Identifier: Apache-2.0

zephyr_interface_library_named(blobs)

if(CONFIG_AIROC_WIFI_CUSTOM)
    zephyr_include_directories(${CMAKE_CURRENT_LIST_DIR}) # for wifi_nvram_image.h
    zephyr_library_sources(4343WA1_clm_blob.c)
    zephyr_library_sources(4343WA1_bin_qspi.c) # use _bin.c for a static build
endif()
