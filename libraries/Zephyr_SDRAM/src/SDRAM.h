#ifndef __SDRAM_H
#define __SDRAM_H

#include "ea_malloc.h"

#ifdef __cplusplus

#include "Arduino.h"

#define SDRAM_END_ADDRESS			(0xc0800000)
#define SDRAM_START_ADDRESS			(0xc0000000)
#define HW_SDRAM_SIZE						(8 * 1024 * 1024)

class SDRAMClass {
public:
	SDRAMClass() {}
	int begin(uint32_t start_address = SDRAM_START_ADDRESS);
	void* malloc(size_t size);
	void free(void* ptr);
	bool test(bool fast = false, Stream& _serial = Serial);
private:

};
extern SDRAMClass SDRAM;

#endif
#endif // __SDRAM_H
