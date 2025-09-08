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

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/video.h>
#include <zephyr/drivers/video-controls.h>

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

#if defined(CONFIG_VIDEO)
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/logging/log.h>

int camera_ext_clock_enable(void) {
	int ret;
	uint32_t rate;
	const struct device *cam_ext_clk_dev = DEVICE_DT_GET(DT_NODELABEL(pwmclock));

	if (!cam_ext_clk_dev) {
		return -ENODEV;		
	}

	(void)zephyr::arduino::init_dev_apply_pinctrl(cam_ext_clk_dev);

	ret = clock_control_on(cam_ext_clk_dev, (clock_control_subsys_t)0);
	if (ret < 0) {
		return ret;
	}

	ret = clock_control_get_rate(cam_ext_clk_dev, (clock_control_subsys_t)0, &rate);
	if (ret < 0) {
		return ret;
	}

	return 0;
}
#else
#ERROR "CONFIG_VIDEO is not defined for this variant"
#endif

bool Camera::begin(uint32_t width, uint32_t height, uint32_t pixformat, bool byte_swap) {
#if DT_HAS_CHOSEN(zephyr_camera)
	this->vdev = DEVICE_DT_GET(DT_CHOSEN(zephyr_camera));
#endif

	// start the clock
	int ret;

	if (!this->vdev) {
		return false;
	}

	if ((ret = camera_ext_clock_enable()) != 0) {
		printk("Camera::begin failed = clock enable: %d\n", ret);
		return false;
	}

	delay(50);

	if ((ret = zephyr::arduino::init_dev_apply_pinctrl(this->vdev)) != 0) {
		printk("Camera::begin failed = vedev: %d\n", ret);
		return false;
	}


	// Now see if the actual camera is defined in choosen. And see if it is ready
#if DT_HAS_CHOSEN(zephyr_camera_sensor)
	const struct device *camera_sensor = DEVICE_DT_GET(DT_CHOSEN(zephyr_camera_sensor));
	if ((ret = zephyr::arduino::init_dev_apply_pinctrl(camera_sensor)) != 0) {
		printk("Camera::begin failed = sensor: %d\n", ret);
		return false;
	}

#endif

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
	struct video_caps caps;
	if (video_get_caps(this->vdev, &caps)) {
		return false;
	}

	for (size_t i = 0; caps.format_caps[i].pixelformat != 0; i++) {
		const struct video_format_cap *fcap = &caps.format_caps[i];
		if (fcap->width_min == width && fcap->height_min == height &&
			fcap->pixelformat == pixformat) {
			break;
		}
		if (caps.format_caps[i + 1].pixelformat == 0) {
			Serial.println("The specified format is not supported");
			return false;
		}
	}

	// Set format.
	static struct video_format fmt = {
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
		this->vbuf[i] = video_buffer_aligned_alloc(fmt.pitch * fmt.height,
												   CONFIG_VIDEO_BUFFER_POOL_ALIGN, K_FOREVER);
		if (this->vbuf[i] == NULL) {
			Serial.println("Failed to allocate video buffers");
			return false;
		}
		video_enqueue(this->vdev, this->vbuf[i]);
	}

	// Start video capture
	if (video_stream_start(this->vdev, VIDEO_BUF_TYPE_OUTPUT)) {
		Serial.println("Failed to start capture");
		return false;
	}

	return true;
}

bool Camera::grabFrame(FrameBuffer &fb, uint32_t timeout) {
	if (this->vdev == NULL) {
		return false;
	}

	if (video_dequeue(this->vdev, &fb.vbuf, K_MSEC(timeout))) {
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
