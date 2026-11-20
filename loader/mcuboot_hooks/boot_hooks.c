/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * MCUboot image access hooks: in a swap build with CONFIG_BOOT_BOOTSTRAP, a
 * permanent update (USB DFU) overwrites the primary slot, a test update (OTA)
 * is swapped and can be reverted.
 *
 * Bootstrap copies the secondary slot over the primary one whenever the
 * primary image is not valid, and swaps otherwise. Reporting the primary
 * image as not valid while a permanent request is pending selects the copy.
 * The secondary image is checked before this matters, and the copy erases the
 * secondary trailer at the end, so the primary image is checked normally
 * again afterwards.
 */

#include "bootutil/bootutil.h"
#include "bootutil/bootutil_public.h"
#include "bootutil/boot_hooks.h"
#include "bootutil/boot_public_hooks.h"
#include "bootutil/fault_injection_hardening.h"
#include "bootutil/image.h"
#include "flash_map_backend/flash_map_backend.h"
#include "sysflash/sysflash.h"

static bool permanent_update_pending(int img_index)
{
	struct boot_swap_state state;

	if (boot_read_swap_state_by_id(FLASH_AREA_IMAGE_SECONDARY(img_index), &state) != 0) {
		return false;
	}

	return state.magic == BOOT_MAGIC_GOOD && state.image_ok == BOOT_FLAG_SET;
}

fih_ret boot_image_check_hook(int img_index, int slot)
{
	if (slot == BOOT_SLOT_PRIMARY && permanent_update_pending(img_index)) {
		FIH_RET(FIH_FAILURE);
	}

	FIH_RET(FIH_BOOT_HOOK_REGULAR);
}

int boot_read_image_header_hook(int img_index, int slot, struct image_header *img_head)
{
	return BOOT_HOOK_REGULAR;
}

int boot_perform_update_hook(int img_index, struct image_header *img_head,
			     const struct flash_area *area)
{
	return BOOT_HOOK_REGULAR;
}

int boot_copy_region_post_hook(int img_index, const struct flash_area *area, size_t size)
{
	return 0;
}

int boot_read_swap_state_primary_slot_hook(int image_index, struct boot_swap_state *state)
{
	return BOOT_HOOK_REGULAR;
}
