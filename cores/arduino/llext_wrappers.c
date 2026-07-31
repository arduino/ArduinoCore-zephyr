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

#ifdef CONFIG_ARM
/*
 * Naked trampoline for compiler ABI helpers (__aeabi_*).
 *
 * ARM RTABI passes doubles in core registers r0:r1 / r2:r3 regardless of
 * -mfloat-abi=hard. A C wrapper compiled with -Os generates "ldr r3, [pc, #0]"
 * which clobbers r3 (the high-word of the second double argument) before the
 * tail-call, corrupting the comparison result.
 *
 * The naked trampoline uses r12 (ip, AAPCS inter-procedure scratch register)
 * which is never an argument register, so r0–r3 and d0–d7 are untouched.
 */
#if defined(__ARM_ARCH_6M__) || (defined(__ARM_ARCH) && __ARM_ARCH < 7)
/* Thumb-1 (Cortex-M0/M0+): movw/movt are Thumb-2 only, use a PC-relative
 * literal pool load via r4 (callee-saved, push/pop balanced). r0-r3 carry
 * the RTABI double arguments and must not be touched.
 */
#define VN(name)                                                                                   \
	__attribute__((naked)) void name(void) {                                                       \
		__asm__("push {r4}\n\t"                                                                    \
				"ldr  r4, =__real_" #name "\n\t"                                                   \
				"mov  r12, r4\n\t"                                                                 \
				"pop  {r4}\n\t"                                                                    \
				"bx   r12");                                                                       \
	}
#else
#define VN(name)                                                                                   \
	__attribute__((naked)) void name(void) {                                                       \
		__asm__("movw r12, #:lower16:__real_" #name "\n\t"                                         \
				"movt r12, #:upper16:__real_" #name "\n\t"                                         \
				"bx   r12");                                                                       \
	}
#endif
#endif

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
W2(char *, strtok, char *, const char *)
W3(void *, memchr, const void *, int, size_t)
W1(char *, strdup, const char *)
W4(void *, memmem, const void *, size_t, const void *, size_t);

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

/* math.h - double */
W1(double, acos, double)
W1(double, asin, double)
W1(double, atan, double)
W2(double, atan2, double, double)
W1(double, cos, double)
W1(double, exp, double)
W1(double, exp2, double)
W2(double, fmod, double, double)
W2(double, ldexp, double, int)
W1(double, log10, double)
W1(double, log2, double)
W1(double, log, double)
W2(double, pow, double, double)
W1(double, sin, double)
W1(double, sqrt, double)
W1(double, tan, double)

/* math.h - float */
W1(float, acosf, float)
W1(float, asinf, float)
W1(float, atanf, float)
W2(float, atan2f, float, float)
W1(float, cosf, float)
W1(float, logf, float)
W1(float, sinf, float)
W1(float, sqrtf, float)
W1(float, tanf, float)

#ifdef CONFIG_ARM
/* ARM compiler ABI helpers: need to preserve all RTABI argument registers */
/* thread pointer access */
VN(__aeabi_read_tp)
/* double arithmetic */
VN(__aeabi_dadd)
VN(__aeabi_dsub)
VN(__aeabi_dmul)
VN(__aeabi_ddiv)
/* double comparisons */
VN(__aeabi_dcmpeq)
VN(__aeabi_dcmplt)
VN(__aeabi_dcmple)
VN(__aeabi_dcmpgt)
VN(__aeabi_dcmpge)
VN(__aeabi_dcmpun)
/* float arithmetic */
VN(__aeabi_fadd)
VN(__aeabi_fsub)
VN(__aeabi_fmul)
VN(__aeabi_fdiv)
/* float comparisons */
VN(__aeabi_fcmpeq)
VN(__aeabi_fcmplt)
VN(__aeabi_fcmple)
VN(__aeabi_fcmpgt)
VN(__aeabi_fcmpge)
VN(__aeabi_fcmpun)
/* double <-> integer conversions */
VN(__aeabi_d2iz)
VN(__aeabi_d2uiz)
VN(__aeabi_d2lz)
VN(__aeabi_i2d)
VN(__aeabi_ui2d)
VN(__aeabi_l2d)
VN(__aeabi_ul2d)
/* float <-> double / integer conversions */
VN(__aeabi_d2f)
VN(__aeabi_f2d)
VN(__aeabi_i2f)
VN(__aeabi_ui2f)
VN(__aeabi_f2iz)
VN(__aeabi_f2uiz)
VN(__aeabi_l2f)
VN(__aeabi_ul2f)
/* integer division */
VN(__aeabi_idiv)
VN(__aeabi_uidiv)
VN(__aeabi_idivmod)
VN(__aeabi_uidivmod)
VN(__aeabi_ldivmod)
VN(__aeabi_uldivmod)
#if defined(__ARM_ARCH_6M__) || (defined(__ARM_ARCH) && __ARM_ARCH < 7)
/*
 * Thumb1 switch-dispatch helpers.
 */
VN(__gnu_thumb1_case_uqi)
VN(__gnu_thumb1_case_sqi)
VN(__gnu_thumb1_case_uhi)
VN(__gnu_thumb1_case_shi)
VN(__gnu_thumb1_case_si)
#endif
#endif

/* stdio.h */
W1(int, puts, const char *)
W1(int, putchar, int)
W4(int, vsnprintf, char *, size_t, const char *, va_list)

/*
 * Variadic stdio functions cannot be expressed with the tail-call wrapper
 * macros above. Provide explicit trampolines that capture the argument list
 * and forward to the corresponding v* form, which is bound at load time.
 * Defining these strong symbols in the core makes them win over the archive
 * members during the sketch link, keeping picolibc's sscanf/vfscanf/vfprintf
 * and the __atod_engine/__atof_engine float helpers out of the sketch llext.
 */
extern int __real_vprintf(const char *, va_list);

int printf(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int ret = __real_vprintf(fmt, ap);
	va_end(ap);
	return ret;
}

extern int __real_vsprintf(char *, const char *, va_list);

int sprintf(char *str, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int ret = __real_vsprintf(str, fmt, ap);
	va_end(ap);
	return ret;
}

int snprintf(char *str, size_t size, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int ret = __real_vsnprintf(str, size, fmt, ap);
	va_end(ap);
	return ret;
}

extern int __real_vsscanf(const char *, const char *, va_list);

int sscanf(const char *str, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int ret = __real_vsscanf(str, fmt, ap);
	va_end(ap);
	return ret;
}

/* Direct va_list variants: same forwarding, exposed under their own names so a
 * sketch calling them directly does not pull picolibc's vfprintf/vfscanf. */
int vprintf(const char *fmt, va_list ap) {
	return __real_vprintf(fmt, ap);
}

int vsprintf(char *str, const char *fmt, va_list ap) {
	return __real_vsprintf(str, fmt, ap);
}

int vsscanf(const char *str, const char *fmt, va_list ap) {
	return __real_vsscanf(str, fmt, ap);
}

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
