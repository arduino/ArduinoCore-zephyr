/*
 * Copyright (c) 2026 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BFM_AB_TEST_H_
#define BFM_AB_TEST_H_

/*
 * Promote the spare BFM slot, if it holds a valid image. Only built when
 * CONFIG_APP_BFM_AB_TEST is set; see bfm_ab_test.c.
 */
void bfm_ab_test(void);

#endif /* BFM_AB_TEST_H_ */
