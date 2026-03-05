/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * This file provides long-call trampolines for the LLEXT extension.
 * Precompiled toolchain libraries emit R_ARM_THM_JUMP24 / R_ARM_THM_CALL
 * relocations for the export libc functions. These have a +/-16 MB range
 * limit which is exceeded when the LLEXT is loaded in a different memory
 * region than the kernel.
 */

#include <stddef.h>
#include <stdarg.h>

/* ret func(void) */
#define W0(ret, name)                                                                              \
	extern ret __real_##name(void);                                                                \
	ret name(void) {                                                                               \
		return __real_##name();                                                                    \
	}

/* ret func(t1) */
#define W1(ret, name, t1)                                                                          \
	extern ret __real_##name(t1);                                                                  \
	ret name(t1 a) {                                                                               \
		return __real_##name(a);                                                                   \
	}

/* ret func(t1, t2) */
#define W2(ret, name, t1, t2)                                                                      \
	extern ret __real_##name(t1, t2);                                                              \
	ret name(t1 a, t2 b) {                                                                         \
		return __real_##name(a, b);                                                                \
	}

/* ret func(t1, t2, t3) */
#define W3(ret, name, t1, t2, t3)                                                                  \
	extern ret __real_##name(t1, t2, t3);                                                          \
	ret name(t1 a, t2 b, t3 c) {                                                                   \
		return __real_##name(a, b, c);                                                             \
	}

/* ret func(t1, t2, t3, t4) */
#define W4(ret, name, t1, t2, t3, t4)                                                              \
	extern ret __real_##name(t1, t2, t3, t4);                                                      \
	ret name(t1 a, t2 b, t3 c, t4 d) {                                                             \
		return __real_##name(a, b, c, d);                                                          \
	}

/* void func(void) */
#define V0(name)                                                                                   \
	extern void __real_##name(void);                                                               \
	void name(void) {                                                                              \
		__real_##name();                                                                           \
	}

/* void func(t1) */
#define V1(name, t1)                                                                               \
	extern void __real_##name(t1);                                                                 \
	void name(t1 a) {                                                                              \
		__real_##name(a);                                                                          \
	}

/* string.h */
W3(void *, memcpy, void *, const void *, size_t)
W3(void *, memmove, void *, const void *, size_t)
W1(size_t, strlen, const char *)
W2(size_t, strnlen, const char *, size_t)
W2(char *, strchr, const char *, int)
W2(char *, strrchr, const char *, int)
W2(char *, strstr, const char *, const char *)
W2(int, strcmp, const char *, const char *)
W3(int, strncmp, const char *, const char *, size_t)
W2(int, strcasecmp, const char *, const char *)
W3(char *, strncpy, char *, const char *, size_t)
W2(char *, strcat, char *, const char *)
W2(char *, strcpy, char *, const char *)
W3(int, memcmp, const void *, const void *, unsigned int)
W3(void *, memset, void *, int, unsigned int)

/* stdlib.h - conversion */
W2(double, strtod, const char *, char **)
W3(long, strtol, const char *, char **, int)
W3(unsigned long, strtoul, const char *, char **, int)
W1(int, atoi, const char *)
W1(double, atof, const char *)
W1(long, atol, const char *)

/* stdlib.h - memory */
W1(void *, malloc, size_t)
W2(void *, realloc, void *, size_t)
W2(void *, calloc, size_t, size_t)
V1(free, void *)

/* stdlib.h - random */
W0(int, rand)
V1(srand, unsigned int)

/* ctype.h */
W1(int, isspace, int)
W1(int, isalnum, int)
W1(int, tolower, int)
W1(int, toupper, int)
W1(int, isalpha, int)
W1(int, iscntrl, int)
W1(int, isdigit, int)
W1(int, isgraph, int)
W1(int, isprint, int)
W1(int, isupper, int)
W1(int, islower, int)
W1(int, isxdigit, int)

/* math.h - double(double) */
W1(double, acos, double)
W1(double, asin, double)
W1(double, atan, double)
W1(double, cos, double)
W1(double, exp, double)
W1(double, exp2, double)
W1(double, log, double)
W1(double, log2, double)
W1(double, log10, double)
W1(double, sin, double)
W1(double, sqrt, double)
W1(double, tan, double)

/* math.h - float(float) */
W1(float, acosf, float)
W1(float, asinf, float)
W1(float, atanf, float)
W1(float, cosf, float)
W1(float, logf, float)
W1(float, sinf, float)
W1(float, sqrtf, float)
W1(float, tanf, float)

/* math.h - double(double, double) */
W2(double, atan2, double, double)
W2(double, pow, double, double)

/* math.h - double(double, int) */
W2(double, ldexp, double, int)

/* math.h - float(float, float) */
W2(float, atan2f, float, float)

/* stdio.h */
W1(int, puts, const char *)
W1(int, putchar, int)
W4(int, vsnprintf, char *, size_t, const char *, va_list)

/* stdlib.h - atexit */
typedef void (*__atexit_fn)(void);
W1(int, atexit, __atexit_fn)

/* process control - noreturn */
extern void __real_abort(void) __attribute__((noreturn));

__attribute__((noreturn)) void abort(void) {
	__real_abort();
	__builtin_unreachable();
}

extern void __real_exit(int) __attribute__((noreturn));

__attribute__((noreturn)) void exit(int status) {
	__real_exit(status);
	__builtin_unreachable();
}

extern void __real__exit(int) __attribute__((noreturn));

__attribute__((noreturn)) void _exit(int status) {
	__real__exit(status);
	__builtin_unreachable();
}
