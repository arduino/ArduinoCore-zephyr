/*
  Camera to Display

  Requirements:
  - An Arduino board with both a supported camera and a display, for example a
    GIGA R1 with the GIGA Display Shield.

  Shows the camera output on the display. The sensor is landscape and the panel
  is portrait, so each frame is rotated 90 degrees clockwise and scaled up by
  whatever whole number fits, then written into the display framebuffer.

  How to use this example:

  1. Change CAMERA_WIDTH / CAMERA_HEIGHT below if you want a different mode.
     The rotation, scale factor and centring all follow from it.

  2. Upload this sketch to the board.

  The frame rate is printed to Serial once a second. Most of the frame time
  goes on the rotation, since neither the LTDC nor DMA2D can transpose.

  Set CONFIG_STM32_LTDC_FB_NUM=2 to avoid tearing, at the cost of one more full
  screen buffer in SDRAM.
*/

#include "camera.h"

#if !DT_HAS_CHOSEN(zephyr_display)
#warning "This example needs a board with a display, it does nothing here."

void setup() {
  Serial.begin(115200);
  Serial.println("This example needs a board with a display.");
}

void loop() {
}

#else

#include "Arduino_Video.h"

// Capture settings.
constexpr int CAMERA_WIDTH  = 320;
constexpr int CAMERA_HEIGHT = 240;

// Two or more buffers means no tearing, one means the frame is drawn into the
// buffer being scanned out. Zero means the application owns them, unsupported.
#define FB_COUNT      CONFIG_STM32_LTDC_FB_NUM

#if FB_COUNT < 1
#error "Set CONFIG_STM32_LTDC_FB_NUM to 1 or more."
#endif

// Use native portrait coordinates; the camera frame is rotated below.
Arduino_Video Display(480, 800, GigaDisplayShield);
static int display_width;
static int display_height;
static int scale;
static int out_x;
static int out_y;

// Rotating 90 degrees swaps the axes.
constexpr int ROT_W = CAMERA_HEIGHT;
constexpr int ROT_H = CAMERA_WIDTH;

// The camera and display buffers both live in external SDRAM, where a strided
// write costs a row activation. Walking the frame in tiles keeps each tile's
// reads and writes inside a few open rows.
constexpr int TILE = 16;

Camera cam;

void fatal_error(const char *msg) {
  pinMode(LED_BUILTIN, OUTPUT);
  while (1) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    Serial.println(msg);
  }
}

/*
 * Rotate clockwise and scale up in one pass: panel pixel (x, y) comes from
 * camera pixel (CAMERA_HEIGHT - 1 - x / scale, y / scale). Iterating over the
 * source instead of the destination keeps the sensor reads sequential and
 * turns the scale into a small block store, which is why nothing here
 * interpolates. The scale is the largest whole number that fits the panel.
 */
static void rotate_and_scale(const uint16_t *src, uint16_t *dst) {
  for (int tx = 0; tx < ROT_W; tx += TILE) {
    int x_end = MIN(tx + TILE, ROT_W);

    for (int ty = 0; ty < ROT_H; ty += TILE) {
      int y_end = MIN(ty + TILE, ROT_H);

      for (int x = tx; x < x_end; x++) {
        const uint16_t *s = src + (CAMERA_HEIGHT - 1 - x) * CAMERA_WIDTH + ty;
        uint16_t *d = dst + (out_y + ty * scale) * display_width + out_x + x * scale;

        for (int y = ty; y < y_end; y++) {
          uint16_t pixel = *s++;

          for (int sy = 0; sy < scale; sy++) {
            for (int sx = 0; sx < scale; sx++) {
              d[sy * display_width + sx] = pixel;
            }
          }

          d += display_width * scale;
        }
      }
    }
  }
}

void setup(void) {
  Serial.begin(115200);

  if (!cam.begin(CAMERA_WIDTH, CAMERA_HEIGHT, CAMERA_RGB565, true)) {
    fatal_error("Camera begin failed");
  }

  cam.setVerticalFlip(false);
  cam.setHorizontalMirror(false);

  if (Display.begin() != 0) {
    fatal_error("Display initialization failed");
  }

  display_width = Display.width();
  display_height = Display.height();
  scale = MIN(display_width / ROT_W, display_height / ROT_H);
  if (scale < 1) {
    fatal_error("Rotated camera frame is larger than the display");
  }
  out_x = (display_width - ROT_W * scale) / 2;
  out_y = (display_height - ROT_H * scale) / 2;

  Serial.print("camera ");
  Serial.print(CAMERA_WIDTH);
  Serial.print("x");
  Serial.print(CAMERA_HEIGHT);
  Serial.print(" -> display ");
  Serial.print(display_width);
  Serial.print("x");
  Serial.print(display_height);
  Serial.print(" rotated, scale ");
  Serial.println(scale);
}

void loop() {
  static unsigned long last_report = 0;
  static unsigned int frames = 0;
  FrameBuffer fb;

  if (!cam.grabFrame(fb, 100)) {
    return;
  }

  uint16_t *dst = (uint16_t *)Display.getDrawBuffer();
  if (dst == nullptr) {
    cam.releaseFrame(fb);
    fatal_error("Display RGB565 buffer unavailable");
  }
  rotate_and_scale((const uint16_t *)fb.getBuffer(), dst);
  cam.releaseFrame(fb);

  if (Display.drawBuffer(0, 0, dst) != 0) {
    fatal_error("Display update failed");
  }

  frames++;
  if (millis() - last_report >= 1000) {
    Serial.print(frames);
    Serial.println(" fps");
    frames = 0;
    last_report = millis();
  }
}

#endif // DT_HAS_CHOSEN(zephyr_display)
