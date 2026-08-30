/*
 * Copyright (c) Arduino s.r.l. and/or its affiliated companies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <Arduino.h>
#include <cmsis_core.h>

extern "C" {
void matrixBegin(void);
void matrixEnd(void);
void matrixPlay(const uint8_t *buf, uint32_t len);
void matrixSetGrayscaleBits(uint8_t _max);
void matrixGrayscaleWrite(const uint8_t *buf);
void matrixWrite(const uint32_t *buf);
};

#if __has_include("ArduinoGraphics.h")
#include <ArduinoGraphics.h>
#define MATRIX_WITH_ARDUINOGRAPHICS
#endif

static inline uint32_t reverse(uint32_t x) {
#if defined(__CORTEX_M) && (__CORTEX_M >= 3U)
	return __RBIT(x);
#else
	x = ((x >> 1) & 0x55555555u) | ((x & 0x55555555u) << 1);
	x = ((x >> 2) & 0x33333333u) | ((x & 0x33333333u) << 2);
	x = ((x >> 4) & 0x0f0f0f0fu) | ((x & 0x0f0f0f0fu) << 4);
	x = ((x >> 8) & 0x00ff00ffu) | ((x & 0x00ff00ffu) << 8);
	x = ((x >> 16) & 0xffffu) | ((x & 0xffffu) << 16);
	return x;
#endif
}

// TODO: this is dangerous, use with care
#define loadSequence(frames)                loadWrapper(frames, sizeof(frames))
#define renderBitmap(bitmap, rows, columns) loadPixels(&bitmap[0][0], rows *columns)
#define endTextAnimation(scrollDirection, anim)                                                    \
	endTextToAnimationBuffer(scrollDirection, anim##_buf, sizeof(anim##_buf), anim##_buf_used)
#define loadTextAnimationSequence(anim) loadWrapper(anim##_buf, anim##_buf_used)

class Arduino_LED_Matrix
#ifdef MATRIX_WITH_ARDUINOGRAPHICS
	: public ArduinoGraphics
#endif
{

public:
	Arduino_LED_Matrix()
#ifdef MATRIX_WITH_ARDUINOGRAPHICS
		: ArduinoGraphics(canvasWidth, canvasHeight)
#endif
	{
	}

	// TODO: find a better name
	// autoscroll will be slower than calling next() at precise times
	void autoscroll(uint32_t interval_ms) {
		_interval = interval_ms;
	}

	int begin() {
		matrixBegin();
		return 1;
	}

	void end() {
		matrixEnd();
	}

	// the irq routine supports 8 levels of grayscale (3 bits), but usually the
	// conversion tools to grayscale will convert to 256 levels (8 bits). Call
	// this accorgingly to your source. The biggest value in the framebuffer
	// should be 2^max_grayscale_bits - 1 (eg. 7 or 255)
	void setGrayscaleBits(uint8_t max) {
		matrixSetGrayscaleBits(max);
	}

	// Plays a video sequence, in grayscale.
	void playVideo(const uint8_t *buf, uint32_t len) {
		matrixPlay(buf, len);
	}

	// Draws a grayscale picture.
	void draw(const uint8_t *buf) {
		matrixGrayscaleWrite(buf);
	}

	void next() {
		uint32_t frame[4];
		frame[0] = reverse(*(_frames + (_currentFrame * 5) + 0));
		frame[1] = reverse(*(_frames + (_currentFrame * 5) + 1));
		frame[2] = reverse(*(_frames + (_currentFrame * 5) + 2));
		frame[3] = reverse(*(_frames + (_currentFrame * 5) + 3));
		_interval = *(_frames + (_currentFrame * 5) + 4);
		_currentFrame = (_currentFrame + 1) % _framesCount;
		if (_currentFrame == 0) {
			if (!_loop) {
				_interval = 0;
			}
			if (_callBack != nullptr) {
				_callBack();
			}
			_sequenceDone = true;
		}
		matrixWrite(frame);
	}

	void loadFrame(const uint32_t buffer[4]) {
		uint32_t tempBuffer[][5] = {{buffer[0], buffer[1], buffer[2], buffer[3], 0}};
		loadSequence(tempBuffer);
		next();
		_interval = 0;
	}

	void renderFrame(uint8_t frameNumber) {
		_currentFrame = frameNumber % _framesCount;
		next();
		_interval = 0;
	}

	void playSequence(bool loop = false) {
		_loop = loop;
		_sequenceDone = false;
		do {
			next();
			delay(_interval);
		} while (_sequenceDone == false);
	}

	bool sequenceDone() {
		if (_sequenceDone) {
			_sequenceDone = false;
			return true;
		}
		return false;
	}

	static void loadPixelsToBuffer(uint8_t *arr, size_t size, uint32_t *dst) {
		uint32_t partialBuffer = 0;
		uint8_t pixelIndex = 0;
		uint8_t *frameP = arr;
		uint32_t *frameHolderP = dst;
		while (pixelIndex / 32 < size / 32) {
			partialBuffer |= *frameP++;
			if ((pixelIndex + 1) % 32 == 0) {
				*(frameHolderP++) = partialBuffer;
			}
			partialBuffer = partialBuffer << 1;
			pixelIndex++;
		}
		// last line, populate partialBuffer in reverse order
		uint8_t remaining = size % 32;
		partialBuffer = 0;
		for (int i = 0; i < remaining; i++) {
			partialBuffer |= (frameP[i] << (31 - i));
		}
		*(frameHolderP++) = partialBuffer;
	}

	void loadPixels(uint8_t *arr, size_t size) {
		loadPixelsToBuffer(arr, size, _frameHolder);
		loadFrame(_frameHolder);
	};

	void loadWrapper(const uint32_t frames[][5], uint32_t howMany) {
		_currentFrame = 0;
		_frames = (uint32_t *)frames;
		_framesCount = (howMany / 5) / sizeof(uint32_t);
	}

	// WARNING: callbacks are fired from ISR. The execution time will be limited.
	void setCallback(voidFuncPtr callBack) {
		_callBack = callBack;
	}

	void clear() {
		const uint32_t fullOff[] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
		loadFrame(fullOff);
#ifdef MATRIX_WITH_ARDUINOGRAPHICS
		memset(_canvasBuffer, 0, sizeof(_canvasBuffer));
#endif
	}

#ifdef MATRIX_WITH_ARDUINOGRAPHICS
	virtual void set(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
		if (y >= canvasHeight || x >= canvasWidth || y < 0 || x < 0) {
			return;
		}
		// the r parameter is (mis)used to set the character to draw with
		_canvasBuffer[y][x] = (r | g | b) > 0 ? 1 : 0;
	}

	void endText(int scrollDirection = NO_SCROLL) {
		ArduinoGraphics::endText(scrollDirection);
		renderBitmap(_canvasBuffer, canvasHeight, canvasWidth);
	}

	// display the drawing or capture it to buffer when rendering dynamic
	// anymation
	void endDraw() {
		ArduinoGraphics::endDraw();

		if (!captureAnimation) {
			renderBitmap(_canvasBuffer, canvasHeight, canvasWidth);
		} else {
			if (captureAnimationHowManyRemains >= 4) {
				loadPixelsToBuffer(&_canvasBuffer[0][0], sizeof(_canvasBuffer),
								   captureAnimationFrame);
				captureAnimationFrame[3] = _textScrollSpeed;
				captureAnimationFrame += 4;
				captureAnimationHowManyRemains -= 16;
			}
		}
	}

	void endTextToAnimationBuffer(int scrollDirection, uint32_t frames[][4], uint32_t howManyMax,
								  uint32_t &howManyUsed) {
		captureAnimationFrame = &frames[0][0];
		captureAnimationHowManyRemains = howManyMax;

		captureAnimation = true;
		ArduinoGraphics::textScrollSpeed(0);
		ArduinoGraphics::endText(scrollDirection);
		ArduinoGraphics::textScrollSpeed(_textScrollSpeed);
		captureAnimation = false;

		howManyUsed = howManyMax - captureAnimationHowManyRemains;
	}

	void textScrollSpeed(unsigned long speed) {
		ArduinoGraphics::textScrollSpeed(speed);
		_textScrollSpeed = speed;
	}

private:
	uint32_t *captureAnimationFrame = nullptr;
	uint32_t captureAnimationHowManyRemains = 0;
	bool captureAnimation = false;
	static const byte canvasWidth = 13;
	static const byte canvasHeight = 8;
	uint8_t _canvasBuffer[canvasHeight][canvasWidth] = {{0}};
	unsigned long _textScrollSpeed = 100;
#endif

private:
	int _currentFrame = 0;
	uint32_t _frameHolder[3];
	uint32_t *_frames;
	uint32_t _framesCount;
	uint32_t _interval = 0;
	uint32_t _lastInterval = 0;
	bool _loop = false;
	bool _sequenceDone = false;
	voidFuncPtr _callBack;
};

// For backwards compatibility, also define the class name without underscores
typedef Arduino_LED_Matrix ArduinoLEDMatrix;
