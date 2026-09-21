/*
  Camera to Serial Streamer

  Requirements:
  - An Arduino board with a supported camera (e.g. Portenta H7, GIGA R1)

  Streams camera frames to a host PC over the USB serial port using a small
  framed protocol, so the viewer discovers the resolution and pixel format
  from the wire instead of having them hardcoded in two places.

  How to use this example:

  1. Change CAMERA_WIDTH / CAMERA_HEIGHT / CAMERA_FORMAT below if you want
     a different mode. Nothing needs to change on the host side.

  2. Upload this sketch to the board.

  3. Make sure no serial monitor or other program is keeping the serial
     port open.

  4. Run the host script provided in the library extras folder:
       extras/CameraSerialViewer/CameraSerialViewer.py
     It opens a live preview window and shows the frame rate and throughput.
*/

#include "camera.h"

// Capture settings. The host reads these from the frame header.
#define CAMERA_WIDTH  320
#define CAMERA_HEIGHT 240
#define CAMERA_FORMAT CAMERA_RGB565

// Set to true to emit little-endian RGB565. The header records which was
// used, so the viewer handles either.
#define CAMERA_BYTE_SWAP false

// Protocol
#define PROTO_VERSION      1
#define PROTO_CMD_REQUEST  'R'
#define PROTO_CMD_STREAM   'S'
#define PROTO_CMD_STOP     'X'
#define PROTO_FLAG_LE      (1 << 0)

struct __attribute__((packed)) FrameHeader {
  uint8_t  sync[4];
  uint8_t  version;
  uint8_t  format;
  uint8_t  flags;
  uint8_t  reserved;
  uint16_t width;
  uint16_t height;
  uint32_t length;
  uint32_t seq;
};

Camera cam;
static uint32_t frame_seq = 0;
static bool streaming = false;

void fatal_error(const char *msg) {
  Serial.println(msg);
  pinMode(LED_BUILTIN, OUTPUT);
  while (1) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
  }
}

void send_frame() {
  FrameBuffer fb;

  if (!cam.grabFrame(fb)) {
    return;
  }

  FrameHeader hdr;
  hdr.sync[0]  = 0xD5;
  hdr.sync[1]  = 0xAA;
  hdr.sync[2]  = 0x96;
  hdr.sync[3]  = 0x5A;
  hdr.version  = PROTO_VERSION;
  hdr.format   = CAMERA_FORMAT;
  hdr.flags    = CAMERA_BYTE_SWAP ? PROTO_FLAG_LE : 0;
  hdr.reserved = 0;
  hdr.width    = CAMERA_WIDTH;
  hdr.height   = CAMERA_HEIGHT;
  hdr.length   = fb.getBufferSize();
  hdr.seq      = frame_seq++;

  Serial.write((uint8_t *)&hdr, sizeof(hdr));
  Serial.write(fb.getBuffer(), fb.getBufferSize());

  cam.releaseFrame(fb);
}

void setup(void) {
  Serial.begin(115200);
  if (!cam.begin(CAMERA_WIDTH, CAMERA_HEIGHT, CAMERA_FORMAT, CAMERA_BYTE_SWAP)) {
    fatal_error("Camera begin failed");
  }
  cam.setVerticalFlip(false);
  cam.setHorizontalMirror(false);
}

void loop() {
  int cmd = Serial.read();

  switch (cmd) {
    case PROTO_CMD_REQUEST:
      send_frame();
      break;
    case PROTO_CMD_STREAM:
      streaming = true;
      break;
    case PROTO_CMD_STOP:
      streaming = false;
      break;
    default:
      if (streaming) {
        send_frame();
      } else {
        // Nothing pending, let other threads run instead of spinning.
        delay(1);
      }
      break;
  }
}
