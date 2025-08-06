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
 */
#ifndef __CAMERA_H__
#define __CAMERA_H__

/**
 * @enum CameraPixelFormat
 * @brief Camera pixel format enumeration.
 *
 * The different formats use different numbers of bits per pixel:
 * - Grayscale (8-bit)
 * - RGB565 (16-bit)
 */
enum CameraPixelFormat {
	CAMERA_RGB565,    /**< RGB565 format (16-bit). */
	CAMERA_GRAYSCALE, /**< Grayscale format (8-bit). */
};

/**
 * @class FrameBuffer
 * @brief Frame buffer class for storing captured frames.
 */
class FrameBuffer {
private:
	struct video_buffer *vbuf;

public:
	/**
	 * @brief Construct a new FrameBuffer object.
	 */
	FrameBuffer();

	/**
	 * @brief Get the buffer size in bytes.
	 * @return uint32_t The buffer size in bytes.
	 */
	uint32_t getBufferSize();

	/**
	 * @brief Get a pointer to the frame buffer.
	 *
	 * This can be used to read the pixels from the frame buffer.
	 * For example, to print all pixels to the serial monitor as hex values.
	 *
	 * @return uint8_t* Pointer to the frame buffer.
	 */
	uint8_t *getBuffer();
	friend class Camera;
};

// bugbug temporary
#include <zephyr/drivers/video.h>

/**
 * @class Camera
 * @brief The main class for controlling a camera.
 */
class Camera {
private:
	bool byte_swap;
	bool yuv_to_gray;
	const struct device *vdev;
	struct video_buffer *vbuf[CONFIG_VIDEO_BUFFER_POOL_NUM_MAX];

public:
	/**
	 * @brief Construct a new Camera object.
	 */
	Camera();

	/**
	 * @brief Initialize the camera.
	 *
	 * @param width Frame width in pixels.
	 * @param height Frame height in pixels.
	 * @param pixformat Initial pixel format (default: CAMERA_RGB565).
	 * @param byte_swap Enable byte swapping (default: false).
	 * @return true if the camera is successfully initialized, otherwise false.
	 */
	bool begin(uint32_t width, uint32_t height, uint32_t pixformat = CAMERA_RGB565,
			   bool byte_swap = false);

	/**
	 * @brief Initialize the camera.
	 *
	 * @param width Frame width in pixels.
	 * @param height Frame height in pixels.
	 * @param crop_width crop width in pixels.
	 * @param crop_height crop height in pixels.
	 * @param pixformat Initial pixel format (default: CAMERA_RGB565).
	 * @param byte_swap Enable byte swapping (default: false).
	 * @return true if the camera is successfully initialized, otherwise false.
	 */
	bool begin(uint32_t width, uint32_t height, uint32_t crop_width, uint32_t crop_height,
			   uint32_t pixformat = CAMERA_RGB565, bool byte_swap = false);

	/**
	 * @brief Capture a frame.
	 *
	 * @param fb Reference to a FrameBuffer object to store the frame data.
	 * @param timeout Time in milliseconds to wait for a frame (default: 5000).
	 * @return true if the frame is successfully captured, otherwise false.
	 */
	bool grabFrame(FrameBuffer &fb, uint32_t timeout = 5000);

	/**
	 * @brief Release a frame buffer.
	 *
	 * @param fb Reference to a FrameBuffer object to release.
	 * @return true if the frame buffer is successfully released, otherwise false.
	 */
	bool releaseFrame(FrameBuffer &fb);

	/**
	 * @brief Flip the camera image vertically.
	 *
	 * @param flip_enable Set to true to enable vertical flip, false to disable.
	 * @return true on success, false on failure.
	 */
	bool setVerticalFlip(bool flip_enable);

	/**
	 * @brief Mirror the camera image horizontally.
	 *
	 * @param mirror_enable Set to true to enable horizontal mirror, false to disable.
	 * @return true on success, false on failure.
	 */
	bool setHorizontalMirror(bool mirror_enable);

	/* Experiments to be able to set crop and the like */
	/**
	 * @brief Set video selection (crop/compose).
	 *
	 * Configure the optional crop and compose feature of a video device.
	 * Crop is first applied on the input frame, and the result of that crop is applied
	 * to the compose. The result of the compose (width/height) is equal to the format
	 * width/height given to the @ref video_set_format function.
	 *
	 * Some targets are inter-dependents. For instance, setting a @ref VIDEO_SEL_TGT_CROP will
	 * reset @ref VIDEO_SEL_TGT_COMPOSE to the same size.
	 *
	 * @param sel Pointer to a video selection structure
	 *
	 * @retval 0 Is successful.
	 * @retval -EINVAL If parameters are invalid.
	 * @retval -ENOTSUP If format is not supported.
	 * @retval -EIO General input / output error.
	 */
	int setSelection(struct video_selection *sel);
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
	int getSelection(struct video_selection *sel);

	/**
	 * @brief returns if snapshot mode is turned on or off.
	 *
	 * @retval true if in snapshot mode false otherwise.
	 */
	bool getSnapshotMode();

	/**
	 * @brief returns if snapshot mode is turned on or off.
	 *
	 * Must be called before begin to take effect.
	 *
	 * @param snap_shot mode if true.
	 *
	 * @retval 0 is successful.
	 */
	int setSnapshotMode(bool snap_shot);
};

#endif // __CAMERA_H__
