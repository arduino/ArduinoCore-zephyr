/*
 * Copyright (c) 2022 Dhruva Gole
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Arduino.h"
#include "zephyr/kernel.h"
#include <cstdint>
#ifdef CONFIG_LLEXT
#include <zephyr/llext/symbol.h>
#endif

#ifdef CONFIG_MULTITHREADING
void start_static_threads();
#endif

// This function will be overwriten by most variants.
void __attribute__((weak)) initVariant(void) {
}

int main(void) {
#if (DT_NODE_HAS_PROP(DT_PATH(zephyr_user), cdc_acm) &&                                            \
	 (CONFIG_USB_CDC_ACM || CONFIG_USBD_CDC_ACM_CLASS))
	Serial.begin(115200);
#endif

	initVariant();

#ifdef CONFIG_MULTITHREADING
	start_static_threads();
#endif

	setup();

	for (;;) {
		loop();
#if 0 //(DT_NODE_HAS_PROP(DT_PATH(zephyr_user), cdc_acm) && CONFIG_USB_CDC_ACM) ||
	  // DT_NODE_HAS_PROP(DT_PATH(zephyr_user), serials)
    if (arduino::serialEventRun) arduino::serialEventRun();
#endif
	}

	return 0;
}

#ifdef CONFIG_LLEXT
LL_EXTENSION_SYMBOL(main);
#endif

/* These magic symbols are provided by the linker.  */
extern void (*__preinit_array_start[])(void) __attribute__((weak));
extern void (*__preinit_array_end[])(void) __attribute__((weak));
extern void (*__init_array_start[])(void) __attribute__((weak));
extern void (*__init_array_end[])(void) __attribute__((weak));

static void __libc_init_array(void) {
	size_t count;
	size_t i;

	count = __preinit_array_end - __preinit_array_start;
	for (i = 0; i < count; i++) {
		__preinit_array_start[i]();
	}

	count = __init_array_end - __init_array_start;
	for (i = 0; i < count; i++) {
		__init_array_start[i]();
	}
}

extern "C" __attribute__((section(".entry_point"), used)) void entry_point(struct k_heap *stack,
																		   size_t stack_size) {
	(void)stack;
	(void)stack_size; // unused

	// copy .data in the right place
	// .bss should already be in the right place
	// call constructors
	extern uintptr_t _sidata;
	extern uintptr_t _sdata;
	extern uintptr_t _edata;
	extern uintptr_t _sbss;
	extern uintptr_t _ebss;
	extern uintptr_t __heap_start;
	extern uintptr_t __heap_end;
	extern uintptr_t kheap_llext_heap;
	extern uintptr_t kheap_llext_heap_size;

	//__asm volatile ("cpsie i");

	printk("System Heap end: %p\n", &__heap_end);
	printk("System Heap start: %p\n", &__heap_start);
	printk("Sketch Heap start: %p, size %p\n", &kheap_llext_heap, &kheap_llext_heap_size);

	memcpy(&_sdata, &_sidata, (&_edata - &_sdata) * sizeof(uint32_t));
	memset(&_sbss, 0, &_ebss - &_sbss);
	__libc_init_array();
	main();
}
