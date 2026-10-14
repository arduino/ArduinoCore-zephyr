# Copyright (c) Arduino s.r.l. and/or its affiliated companies
# SPDX-License-Identifier: Apache-2.0

if(SB_CONFIG_ENFORCE_SECURE_WATCHDOG)
	# MCUboot starts the watchdog...
	set_config_bool(mcuboot CONFIG_WATCHDOG y)
	set_config_bool(mcuboot CONFIG_BOOT_WATCHDOG_SETUP_AT_BOOT y)
	set_config_bool(mcuboot CONFIG_BOOT_WATCHDOG_INSTALL_TIMEOUT_AT_BOOT y)
	set_config_bool(mcuboot CONFIG_BOOT_USB_DFU_AVOID_WATCHDOG y)
	set_config_int(mcuboot CONFIG_BOOT_WATCHDOG_TIMEOUT_MS ${SB_CONFIG_SECURE_WATCHDOG_TIMEOUT_MS})

	# ...and the loader feeds it with the same timeout.
	set_config_bool(${DEFAULT_IMAGE} CONFIG_WATCHDOG y)
	set_config_bool(${DEFAULT_IMAGE} CONFIG_APP_WATCHDOG_ENFORCED y)
	set_config_int(${DEFAULT_IMAGE} CONFIG_APP_WATCHDOG_TIMEOUT_MS ${SB_CONFIG_SECURE_WATCHDOG_TIMEOUT_MS})
endif()

if(SB_CONFIG_ARDUINO_MCUBOOT_HOOKS)
	set(mcuboot_EXTRA_ZEPHYR_MODULES ${ZEPHYR_MCUBOOT_MODULE_DIR}/boot/arduino/hooks
	    CACHE INTERNAL "Arduino MCUboot hooks module")
endif()

# Like the loader's own .conf/.overlay, a variant keeps its MCUboot
# configuration in its directory.
set(arduino_variant_dir ${APP_DIR}/../variants/${NORMALIZED_BOARD_TARGET})
if(EXISTS ${arduino_variant_dir}/mcuboot.conf)
	set(mcuboot_EXTRA_CONF_FILE ${arduino_variant_dir}/mcuboot.conf
	    CACHE INTERNAL "MCUboot configuration of this variant")
endif()
if(EXISTS ${arduino_variant_dir}/mcuboot.overlay)
	set(mcuboot_EXTRA_DTC_OVERLAY_FILE ${arduino_variant_dir}/mcuboot.overlay
	    CACHE INTERNAL "MCUboot devicetree overlay of this variant")
endif()
