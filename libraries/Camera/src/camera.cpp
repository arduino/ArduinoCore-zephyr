/*
 * Copyright 2025 Arduino SA
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
	return nullptr;
}

Camera::Camera()
	: vdev(NULL), byte_swap(false), yuv_to_gray(false),
	  snapshot_mode(CONFIG_VIDEO_BUFFER_POOL_NUM_MAX <= 1) {
	for (size_t i = 0; i < ARRAY_SIZE(this->vbuf); i++) {
		this->vbuf[i] = NULL;
	}
}

bool Camera::begin(uint32_t width, uint32_t height, uint32_t pixformat, bool byte_swap) {
	return begin(width, height, width, height, pixformat, byte_swap);
}

bool Camera::begin(uint32_t width, uint32_t height, uint32_t crop_width, uint32_t crop_height,
				   uint32_t pixformat, bool byte_swap) {
#if DT_HAS_CHOSEN(zephyr_camera)
	this->vdev = DEVICE_DT_GET(DT_CHOSEN(zephyr_camera));
#endif

	if (!this->vdev || !device_is_ready(this->vdev)) {
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
	struct video_caps caps;
	if (video_get_caps(this->vdev, &caps)) {
		return false;
	}

	for (size_t i = 0; caps.format_caps[i].pixelformat != 0; i++) {
		const struct video_format_cap *fcap = &caps.format_caps[i];
		if (fcap->width_min <= width && fcap->width_max >= width && fcap->height_min <= height &&
			fcap->height_max >= height && fcap->pixelformat == pixformat) {
			break;
		}
		if (caps.format_caps[i + 1].pixelformat == 0) {
			Serial.println("The specified format is not supported");
			return false;
		}
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

	// optionally set the crop values
	if (width != crop_width || height != crop_height) {
		struct video_selection vselCrop;
		vselCrop.type = VIDEO_BUF_TYPE_OUTPUT;
		vselCrop.target = VIDEO_SEL_TGT_CROP;
		vselCrop.rect.left = (width - crop_width) / 2;
		vselCrop.rect.top = (height - crop_height) / 2;
		vselCrop.rect.width = crop_width;
		vselCrop.rect.height = crop_height;
		;

		int ret;
		if ((ret = setSelection(&vselCrop)) != 0) {
			printk("ERROR: %d\n", ret);
		}
	}
	// this should compute the sizes needed.
	video_get_format(this->vdev, &fmt);

	// If we are in snapshot mode, try starting the video stream with no buffers
	// to tell it that we want snapshot...
	if (snapshot_mode) {
		if (video_stream_start(this->vdev, VIDEO_BUF_TYPE_OUTPUT)) {
			Serial.println("Snapshot mode Failed to start capture");
			// return false;
		}
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
	if (!snapshot_mode) {
		if (video_stream_start(this->vdev, VIDEO_BUF_TYPE_OUTPUT)) {
			Serial.println("Failed to start capture");
			return false;
		}
	}
	return true;
}

bool Camera::grabFrame(FrameBuffer &fb, uint32_t timeout) {
	if (this->vdev == NULL) {
		return false;
	}
	// printk("Camera::grabFrame called\n");
	if (video_dequeue(this->vdev, &fb.vbuf, K_MSEC(timeout))) {
		return false;
	}
	// printk("video_dequeue returned :%p\n", fb.vbuf->buffer);
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

	int ret;
	// printk("Camera::ReleaseFrame called\n");
	if ((ret = video_enqueue(this->vdev, fb.vbuf)) != 0) {
		printk("Failed to enqueue buffer %d\n", ret);
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

int Camera::setSelection(struct video_selection *sel) {
	return video_set_selection(vdev, sel);
}

/**
 * @brief Get video selection (crop/compose).
 *
 * Retrieve the current settings related to the crop and compose of the video device.
 * This can also be used to read the native size of the input stream of the video
 * device.
 * This function can be used to read crop / compose capabilities of the device prior
 * to performing configuration via the @ref video_set_selection api.
 *
 * @param sel Pointer to a video selection structure, @c type and @c target set by the caller
 *
 * @retval 0 Is successful.
 * @retval -EINVAL If parameters are invalid.
 * @retval -ENOTSUP If format is not supported.
 * @retval -EIO General input / output error.
 */
int Camera::getSelection(struct video_selection *sel) {
	return video_get_selection(vdev, sel);
}

/**
 * @brief returns if snapshot mode is turned on or off.
 *
 * @param snapshot_mode pointer to Turn Snaphsot mode on or off..
 */
bool Camera::getSnapshotMode() {
	return snapshot_mode;
}

/**
 * @brief returns if snapshot mode is turned on or off.
 *
 * Must be called before begin to take effect.
 *
 * @param snap_shot mode if true.
 *
 * @retval 0 is successful.
 */
int Camera::setSnapshotMode(bool snap_shot) {
	if (snap_shot) {
		snapshot_mode = snap_shot;
		return 0;
	} else {
#if CONFIG_VIDEO_BUFFER_POOL_NUM_MAX <= 1
		return -EINVAL;
#endif
		snapshot_mode = snap_shot;
		return 0;
	}
}
