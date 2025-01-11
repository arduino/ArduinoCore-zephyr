#include "SDRAM.h"
#include "Arduino.h"

int SDRAMClass::begin(uint32_t start_address) {
	if (start_address) {
		malloc_addblock((void*)start_address, SDRAM_END_ADDRESS - start_address);
	}

	return 1;
}

void* SDRAMClass::malloc(size_t size) {
	return ea_malloc(size);
}

void SDRAMClass::free(void* ptr) {
	ea_free(ptr);
}

bool SDRAMClass::test(bool fast, Stream& _serial) {
    uint8_t const pattern = 0xaa;
    uint8_t const antipattern = 0x55;
    uint8_t *const mem_base = (uint8_t*)SDRAM_START_ADDRESS;

    /* test data bus */
    for (uint8_t i = 1; i; i <<= 1) {
        *mem_base = i;
        if (*mem_base != i) {
            _serial.println("data bus lines test failed! data (" + String(i) + ")");
            __asm__ volatile ("BKPT");
        }
    }

    /* test address bus */
    /* Check individual address lines */
    for (uint32_t i = 1; i < HW_SDRAM_SIZE; i <<= 1) {
        mem_base[i] = pattern;
        if (mem_base[i] != pattern) {
            _serial.println("address bus lines test failed! address ("+ String((uint32_t)&mem_base[i], HEX) + ")");
            __asm__ volatile ("BKPT");
        }
    }

    /* Check for aliasing (overlaping addresses) */
    mem_base[0] = antipattern;
    for (uint32_t i = 1; i < HW_SDRAM_SIZE; i <<= 1) {
        if (mem_base[i] != pattern) {
            _serial.println("address bus overlap! address ("+ String((uint32_t)&mem_base[i], HEX) + ")");
            __asm__ volatile ("BKPT");
        }
    }

    /* test all ram cells */
    if (!fast) {
        for (uint32_t i = 0; i < HW_SDRAM_SIZE; ++i) {
            mem_base[i] = pattern;
            if (mem_base[i] != pattern) {
                _serial.println("address bus test failed! address ("+ String((uint32_t)&mem_base[i], HEX) + ")");
                __asm__ volatile ("BKPT");
            }
        }
    } else {
        memset(mem_base, pattern, HW_SDRAM_SIZE);
    }

    return true;
}

SDRAMClass SDRAM;