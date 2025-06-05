#include <zephyr/kernel.h>

void _on_1200_bps() {
    uint32_t tmp = (uint32_t) & (RTC->BKP0R);
    tmp += (RTC_BKP_DR0 * 4U);
    *(__IO uint32_t *)tmp = (uint32_t)0xDF59;
    NVIC_SystemReset();
}

#if defined(CONFIG_BOARD_ARDUINO_GIGA_R1)  && defined(CONFIG_INPUT_GT911_INTERRUPT)
extern "C" void registerGigaTouchCallback(void (*cb)(struct input_event *evt, void *user_data));
void initVariant(void) {
    // Make sure to set to NULL in case previous sketch or pvevious build of sketch 
    // set a callback, whoes pointer may not be valid
    registerGigaTouchCallback(nullptr);
}
#endif
