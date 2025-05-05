#include "Arduino.h"
#include <hal/nrf_power.h>
#include <zephyr/dt-bindings/gpio/nordic-nrf-gpio.h>

void _on_1200_bps() {
    nrf_power_gpregret_set(NRF_POWER, 0, 0xb0);
    NVIC_SystemReset();
}

void initVariant(void) {
  static const struct gpio_dt_spec enable_sensors =
    GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), pin_enable_gpios);
  if (gpio_is_ready_dt(&enable_sensors)) {
      gpio_flags_t flags = enable_sensors.dt_flags | GPIO_OUTPUT_HIGH | GPIO_ACTIVE_HIGH | NRF_GPIO_DRIVE_H0H1;

      gpio_pin_configure(enable_sensors.port, enable_sensors.pin, flags);
      gpio_pin_set(enable_sensors.port, enable_sensors.pin, HIGH);

      delay(500);
  }
}
