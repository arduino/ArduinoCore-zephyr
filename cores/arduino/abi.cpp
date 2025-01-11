/*
 * Copyright (c) 2024 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>

extern "C" void __cxa_pure_virtual(void) {}
extern "C" void __cxa_deleted_virtual(void) {}
extern "C" int __cxa_atexit(void (*func) (void *), void * arg, void * dso_handle) {
  return 0;
}

namespace std {
  void __throw_length_error(const char* __s __attribute__((unused))) {}
};

extern "C" int strcmp(const char* s1, const char* s2) {
  while(*s1 && (*s1 == *s2))
  {
    s1++;
    s2++;
  }
  return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
