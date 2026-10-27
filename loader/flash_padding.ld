#include <zephyr/devicetree.h>

#if defined(CONFIG_FLASH_ALIGN_BASED_ON_DT_ERASE_DIM)
    #if !DT_HAS_CHOSEN(zephyr_flash)
        #error "Padding failed: No 'zephyr,flash' chosen node defined in Devicetree!"
    #endif

    /* Verify that specific flash node has the erase-block-size property */
    #if !DT_NODE_HAS_PROP(DT_CHOSEN(zephyr_flash), erase_block_size)
        #error "Padding failed: The chosen flash node is missing 'erase-block-size'!"
    #endif

    #define PADDING_DIM DT_PROP(DT_CHOSEN(zephyr_flash), erase_block_size)

#elif defined(CONFIG_CUSTOM_FLASH_ALIGN_ACTIVE)
    #define PADDING_DIM CONFIG_CUSTOM_FLASH_ALIGN
#endif

/* Pad the final image to an exact multiple of the flash erase-block size.
 *
 * zephyr_linker_sources(SECTIONS SORT_KEY zzz_flash_padding ...) (see
 * loader/CMakeLists.txt) places this section last among all
 * SECTIONS-injected snippets, and CONFIG_LINKER_LAST_SECTION_ID=n guarantees 
 * that Zephyr's own mandatory ".last_section", which always follows, stays 
 * empty. That makes this section the true end
 * of the image.
 *
 * BYTE(0xFF) forces the section to contain real data (PROGBITS); a section
 * made up solely of a location-counter advance would be emitted as NOBITS
 * and silently dropped from the final .bin/.hex. FILL(0xFF) then pads
 * everything up to the next FLASH_BLOCK_SIZE boundary with 0xFF.
 */
SECTION_PROLOGUE(.flash_padding,,)
{
    FILL(0xFF);
    BYTE(0xFF);
    . = ALIGN(PADDING_DIM);
} GROUP_LINK_IN(ROMABLE_REGION)
