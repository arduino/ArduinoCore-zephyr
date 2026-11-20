# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

# <app_root>/sysbuild.cmake
if(SB_CONFIG_ENFORCE_SECURE_WATCHDOG)
    set_config_bool(mcuboot CONFIG_WATCHDOG y)
    # Enable the startup features in MCUBoot
    set_config_bool(mcuboot CONFIG_BOOT_WATCHDOG_SETUP_AT_BOOT y)
    set_config_bool(mcuboot CONFIG_BOOT_WATCHDOG_INSTALL_TIMEOUT_AT_BOOT y)
    
    # Inject the timing value into MCUBoot
    set_config_int(mcuboot CONFIG_BOOT_WATCHDOG_TIMEOUT_MS ${SB_CONFIG_SECURE_WATCHDOG_TIMEOUT_MS})
    
    # Inject the flags into the Application
    set_config_bool(${DEFAULT_IMAGE} CONFIG_WATCHDOG y)
    set_config_bool(${DEFAULT_IMAGE} CONFIG_APP_WATCHDOG_ENFORCED y)
    set_config_int(${DEFAULT_IMAGE} CONFIG_APP_WATCHDOG_TIMEOUT_MS ${SB_CONFIG_SECURE_WATCHDOG_TIMEOUT_MS})
endif()

