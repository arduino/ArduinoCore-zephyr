/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

extern "C" {

void *__dso_handle = (void *)&__dso_handle;

void __cxa_pure_virtual(void) {
}

void __cxa_deleted_virtual(void) {
}

int __cxa_atexit(void (*func)(void *), void *arg, void *dso_handle) {
	(void)func;
	(void)arg;
	(void)dso_handle; // unused
	return 0;
}

// C++ exception ABI / unwinder stubs.
//
// Sketches are compiled with -fno-exceptions, but the precompiled libstdc++
// archive is not. Using std::string / std::error_code pulls in objects such as
// functexcept.o and system_error.o whose throw and cleanup paths still reference
// the full exception-handling and stack-unwinding ABI:
//   - libstdc++ eh_*.o  (__cxa_throw, __cxa_*catch, __cxa_*cleanup, ...)
//   - libgcc unwinder    (_Unwind_*, __aeabi_unwind_cpp_pr0/1/2,
//                         __gnu_Unwind_Find_exidx, __exidx_start/__exidx_end)
//
// A sketch is loaded as an llext (relocatable object) at runtime. The llext
// loader resolves undefined symbols against the firmware's export table; the
// firmware exports none of the unwinder symbols above, so loading fails with
// "Undefined symbol ... __gnu_Unwind_Find_exidx / __exidx_start / __exidx_end".
// Pulling the libgcc unwinder in (e.g. via -lgcc) does not help: its personality
// routines reference __gnu_Unwind_Find_exidx and the linker-defined __exidx_*
// bounds, which simply do not exist in a relocatable llext.
//
// Providing strong stubs for the whole exception ABI here keeps every libstdc++
// eh_*.o and the libgcc unwinder OUT of the sketch, so the llext is fully
// self-contained and links/loads cleanly. None of these can be reached at
// runtime because no exception is ever actually thrown; abort() guards misuse.
//
// The link recipe forces this object to be pulled before libstdc++ is scanned
// (see -Wl,-u,__cxa_allocate_exception in platform.txt) so these definitions win
// over the archive members instead of colliding with them.
void *__cxa_allocate_exception(size_t thrown_size) {
	(void)thrown_size; // unused
	abort();
}

void __cxa_free_exception(void *thrown_exception) {
	(void)thrown_exception; // unused
}

void *__cxa_allocate_dependent_exception(void) {
	abort();
}

void __cxa_free_dependent_exception(void *dependent_exception) {
	(void)dependent_exception; // unused
}

void __cxa_throw(void *thrown_exception, void *tinfo, void (*dest)(void *)) {
	(void)thrown_exception;
	(void)tinfo;
	(void)dest;
	abort();
}

void __cxa_rethrow(void) {
	abort();
}

void *__cxa_begin_catch(void *exc) {
	(void)exc;
	abort();
}

void __cxa_end_catch(void) {
}

void __cxa_begin_cleanup(void *exc) {
	(void)exc;
	abort();
}

void __cxa_end_cleanup(void) {
	abort();
}

void *__cxa_type_match(void *exc, const void *catch_type, bool is_ref, void **obj) {
	(void)exc;
	(void)catch_type;
	(void)is_ref;
	(void)obj;
	abort();
}

void __cxa_call_unexpected(void *exc) {
	(void)exc;
	abort();
}

int __gxx_personality_v0(int version, int actions, unsigned exc_class, void *exc, void *ctx) {
	(void)version;
	(void)actions;
	(void)exc_class;
	(void)exc;
	(void)ctx;
	abort();
}

void _Unwind_Resume(void *exc) {
	(void)exc;
	abort();
}

// ARM EABI compact-model personality routines referenced by the exception
// tables of the libstdc++ objects above. Stubbing them prevents the libgcc
// unwinder (and its __gnu_Unwind_Find_exidx / __exidx_* references) from being
// pulled in.
int __aeabi_unwind_cpp_pr0(void) {
	abort();
}

int __aeabi_unwind_cpp_pr1(void) {
	abort();
}

int __aeabi_unwind_cpp_pr2(void) {
	abort();
}

// picolibc strerror() hook. std::error_code::message() (system_error.o) calls
// __xpg_strerror_r() which references the weak picolibc hook _user_strerror.
// It is not provided by the firmware, so define it here (no custom error
// strings -> fall back to picolibc's built-in table) to keep the llext
// self-contained.
char *_user_strerror(int errnum, int internal, int *errptr) {
	(void)errnum;
	(void)internal;
	(void)errptr;
	return 0;
}

} /* extern "C" */
