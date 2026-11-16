/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 * SPDX-License-Identifier: LGPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Camera driver.
 */
#include "Arduino.h"
#include "camera.h"

#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree/port-endpoint.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/video.h>
#include <zephyrClockInit.hpp>
#include <zephyrPinctrl.h>

#if !DT_HAS_CHOSEN(zephyr_camera)
#error "zephyr,camera node is not defined in devicetree"
#endif

#define CLOCK_NODE DT_CHOSEN(arduino_camera_clock)
static const struct pwm_dt_spec CLOCK_PWM = PWM_DT_SPEC_GET(CLOCK_NODE);

#define CAMERA_NODE          DT_CHOSEN(zephyr_camera)
#define CAMERA_PORT_NODE     DT_CHILD(CAMERA_NODE, port)
#define CAMERA_ENDPOINT_NODE DT_CHILD(CAMERA_PORT_NODE, endpoint)
#define CAMERA_SENSOR_NODE   DT_NODE_REMOTE_DEVICE(CAMERA_ENDPOINT_NODE)

#ifdef ARDUINO_CAMERA_USE_MALLOC
/*
 * Video buffers allocated from the C library heap, so that malloc() knows they are in use.
 * The block is over-allocated and aligned by hand (aligned_alloc() is not exported to
 * sketches): `raw` is the pointer returned by malloc(), `data` the aligned buffer. Start and
 * size are rounded to `align` (the cache line size) so that the cache maintenance done for the
 * camera DMA never touches a line shared with other heap data.
 */
struct mem_block {
	void *raw;
	void *data;
};

static struct mem_block user_video_block[CONFIG_VIDEO_BUFFER_POOL_NUM_MAX];

static struct video_buffer *camera_buffer_aligned_alloc(size_t size, size_t align) {
	struct video_buffer *vbuf = NULL;
	struct mem_block *block = NULL;
	unsigned int i;

	if ((align & (align - 1)) != 0) {
		return NULL;
	}

	for (i = 0; i < ARRAY_SIZE(user_video_block); i++) {
		if (user_video_block[i].data == NULL) {
			block = &user_video_block[i];
			break;
		}
	}

	if (block == NULL) {
		return NULL;
	}

	size = ROUND_UP(size, align);
	block->raw = malloc(size + align - 1);
	if (block->raw == NULL) {
		return NULL;
	}
	block->data = (void *)ROUND_UP((uintptr_t)block->raw, align);

	vbuf = video_import_buffer((uint8_t *)block->data, size);
	if (vbuf == NULL) {
		free(block->raw);
		block->raw = NULL;
		block->data = NULL;
		return NULL;
	}

	vbuf->type = VIDEO_BUF_TYPE_OUTPUT;
	vbuf->memory = VIDEO_MEMORY_EXTERNAL;
	vbuf->size = size;
	vbuf->bytesused = 0;
	vbuf->timestamp = 0;
	vbuf->line_offset = 0;

	return vbuf;
}

static int camera_buffer_release(struct video_buffer *vbuf) {
	struct mem_block *block = NULL;
	unsigned int i;

	if (vbuf == NULL) {
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(user_video_block); i++) {
		if (user_video_block[i].data == vbuf->buffer) {
			block = &user_video_block[i];
			break;
		}
	}

	if (block == NULL || block->raw == NULL) {
		return -EINVAL;
	}

	int ret = video_buffer_release(vbuf);
	if (ret != 0) {
		return ret;
	}

	free(block->raw);
	block->raw = NULL;
	block->data = NULL;

	return 0;
}
#endif

FrameBuffer::FrameBuffer() : vbuf(NULL) {
}

uint32_t FrameBuffer::getBufferSize() {
	if (this->vbuf) {
		return this->vbuf->bytesused;
	}

	return 0;
}

uint8_t *FrameBuffer::getBuffer() {
	if (this->vbuf) {
		return this->vbuf->buffer;
	}

	return NULL;
}

Camera::Camera() : byte_swap(false), yuv_to_gray(false), vdev(NULL) {
	for (size_t i = 0; i < ARRAY_SIZE(this->vbuf); i++) {
		this->vbuf[i] = NULL;
	}
}

bool Camera::begin(uint32_t width, uint32_t height, uint32_t pixformat, bool byte_swap) {
	if (zephyr::arduino::init_pwm_ref_clock(DEVICE_DT_GET(CLOCK_NODE), CLOCK_PWM) != 0) {
		return false;
	}

	// init camera sensor first
	const struct device *sensor = DEVICE_DT_GET(CAMERA_SENSOR_NODE);
	if (zephyr::arduino::init_dev_apply_pinctrl(sensor) < 0) {
		return false;
	}

	this->vdev = DEVICE_DT_GET(CAMERA_NODE);

	if (zephyr::arduino::init_dev_apply_pinctrl(this->vdev) < 0) {
		return false;
	}

	switch (pixformat) {
	case CAMERA_RGB565:
		this->byte_swap = byte_swap;
		pixformat = VIDEO_PIX_FMT_RGB565;
		break;
	case CAMERA_GRAYSCALE:
		// There's no support for mono sensors.
		this->yuv_to_gray = true;
		pixformat = VIDEO_PIX_FMT_YUYV;
		break;
	default:
		break;
	}

	// Get capabilities
	struct video_caps caps = {
		.type = VIDEO_BUF_TYPE_OUTPUT,
	};
	int ret = video_get_caps(this->vdev, &caps);
	if (ret != 0) {
		return false;
	}

	bool supported = false;

	for (size_t i = 0; caps.format_caps[i].pixelformat != 0; i++) {
		const struct video_format_cap *fcap = &caps.format_caps[i];

		if (fcap->pixelformat != pixformat) {
			continue;
		}

		// A capability can describe a single size, where min and max are equal,
		// or a range the sensor can be windowed to, in increments of the step.
		if (width < fcap->width_min || width > fcap->width_max || height < fcap->height_min ||
			height > fcap->height_max) {
			continue;
		}

		if (fcap->width_step != 0 && (width - fcap->width_min) % fcap->width_step != 0) {
			continue;
		}

		if (fcap->height_step != 0 && (height - fcap->height_min) % fcap->height_step != 0) {
			continue;
		}

		supported = true;
		break;
	}

	if (!supported) {
		Serial.println("The specified format is not supported");
		return false;
	}

	// Set format.
	static struct video_format fmt = {
		.type = VIDEO_BUF_TYPE_OUTPUT,
		.pixelformat = pixformat,
		.width = width,
		.height = height,
		.pitch = width * 2,
	};

	if (video_set_format(this->vdev, &fmt)) {
		Serial.println("Failed to set video format");
		return false;
	}

	// Allocate video buffers.
	for (size_t i = 0; i < ARRAY_SIZE(this->vbuf); i++) {
#ifdef ARDUINO_CAMERA_USE_MALLOC
		this->vbuf[i] =
			camera_buffer_aligned_alloc(fmt.pitch * fmt.height, CONFIG_VIDEO_BUFFER_POOL_ALIGN);
#else
		this->vbuf[i] = video_buffer_aligned_alloc(fmt.pitch * fmt.height,
												   CONFIG_VIDEO_BUFFER_POOL_ALIGN, K_NO_WAIT);
#endif
		if (this->vbuf[i] == NULL) {
			Serial.println("Failed to allocate video buffers number: " + String(i));
			// No buffers have been queued yet, so these can be released safely.
			for (size_t j = 0; j < i; j++) {
#ifdef ARDUINO_CAMERA_USE_MALLOC
				int release_ret = camera_buffer_release(this->vbuf[j]);
#else
				int release_ret = video_buffer_release(this->vbuf[j]);
#endif
				if (release_ret != 0) {
					return false;
				}
				this->vbuf[j] = NULL;
			}
			return false;
		}
	}

	for (size_t i = 0; i < ARRAY_SIZE(this->vbuf); i++) {
		this->vbuf[i]->type = VIDEO_BUF_TYPE_OUTPUT;
		ret = video_enqueue(this->vdev, this->vbuf[i]);
		if (ret != 0) {
			return false;
		}
	}

	// Start video capture
	ret = video_stream_start(this->vdev, VIDEO_BUF_TYPE_OUTPUT);
	if (ret != 0) {
		Serial.println("Failed to start capture: " + String(ret));
		return false;
	}

	return true;
}

bool Camera::grabFrame(FrameBuffer &fb, uint32_t timeout) {
	if (this->vdev == NULL) {
		return false;
	}

	int ret = video_dequeue(this->vdev, &fb.vbuf, K_MSEC(timeout));
	if (ret != 0) {
		return false;
	}

	if (this->byte_swap) {
		uint16_t *pixels = (uint16_t *)fb.vbuf->buffer;
		for (size_t i = 0; i < fb.vbuf->bytesused / 2; i++) {
			pixels[i] = __REVSH(pixels[i]);
		}
	}

	if (this->yuv_to_gray) {
		uint8_t *pixels = (uint8_t *)fb.vbuf->buffer;
		for (size_t i = 0; i < fb.vbuf->bytesused / 2; i++) {
			pixels[i] = pixels[i * 2];
		}
		fb.vbuf->bytesused /= 2;
	}

	return true;
}

bool Camera::releaseFrame(FrameBuffer &fb) {
	if (this->vdev == NULL) {
		return false;
	}

	fb.vbuf->type = VIDEO_BUF_TYPE_OUTPUT;

	if (video_enqueue(this->vdev, fb.vbuf)) {
		return false;
	}

	return true;
}

bool Camera::setVerticalFlip(bool flip_enable) {
	struct video_control ctrl = {.id = VIDEO_CID_VFLIP, .val = flip_enable};
	return video_set_ctrl(this->vdev, &ctrl) == 0;
}

bool Camera::setHorizontalMirror(bool mirror_enable) {
	struct video_control ctrl = {.id = VIDEO_CID_HFLIP, .val = mirror_enable};
	return video_set_ctrl(this->vdev, &ctrl) == 0;
}
