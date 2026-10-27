# <app_root>/sysbuild.cmake

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
