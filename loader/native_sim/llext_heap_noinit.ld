/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* The posix linker script does not place the LLEXT heap sections */
*(.llext_heap)
*(.llext_ext_heap)
*(.llext_metadata_heap)
