#ifndef _KERNEL_VERSION_H_
#define _KERNEL_VERSION_H_

/* The template values come from cmake/version.cmake
 * BUILD_VERSION related template values will be 'git describe',
 * alternatively user defined BUILD_VERSION.
 */

#define ZEPHYR_VERSION_CODE 262499
#define ZEPHYR_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

#define KERNELVERSION                   0x4016300
#define KERNEL_VERSION_NUMBER           0x40163
#define KERNEL_VERSION_MAJOR            4
#define KERNEL_VERSION_MINOR            1
#define KERNEL_PATCHLEVEL               99
#define KERNEL_TWEAK                    0
#define KERNEL_VERSION_STRING           "4.1.99"
#define KERNEL_VERSION_EXTENDED_STRING  "4.1.99+0"
#define KERNEL_VERSION_TWEAK_STRING     "4.1.99+0"

#define BUILD_VERSION v4.1.0-898-g6b32f4d75066


#endif /* _KERNEL_VERSION_H_ */
