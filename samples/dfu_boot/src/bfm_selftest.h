/*
 * Copyright (c) 2026 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BFM_SELFTEST_H_
#define BFM_SELFTEST_H_

/*
 * Program two scratch BFM pages from the running image and report to the
 * console. Only built when CONFIG_APP_BFM_SELFTEST is set; see bfm_selftest.c.
 */
void bfm_selftest(void);

#endif /* BFM_SELFTEST_H_ */
