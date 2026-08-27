/*
 * Copyright (c) 2025 Arduino SA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DFU_BOOT_DFU_IMAGES_H_
#define DFU_BOOT_DFU_IMAGES_H_

/**
 * @brief Report that a DFU transfer just touched an image.
 *
 * Implemented in main.c, where it re-arms the "DFU has gone quiet, boot the new
 * image" timer.
 */
void dfu_activity_notify(void);

#endif /* DFU_BOOT_DFU_IMAGES_H_ */
