/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* 
 * Stop the build if Zephyr iterable section macros are used in the code. This
 * is not supported by LLEXT and will cause silent issues in the final binary.
 */

SECTIONS {
  .unexpected_iterables (INFO) : { *(SORT(._*.static.*_)) }
  ASSERT(SIZEOF(.unexpected_iterables) == 0,
         "error: Zephyr macros that define static objects are not supported in sketches. Use runtime allocation instead."
  )
}
