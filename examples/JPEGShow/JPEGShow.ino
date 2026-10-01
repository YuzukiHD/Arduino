// Decodes a JPEG with the video engine and shows it on the display; then
// decodes it again and converts a few pixels on the CPU.
#include <VideoDecoder.h>

extern const uint8_t test_jpeg[];
extern const unsigned test_jpeg_size;

void setup() {
  Serial.begin(115200);
  Serial.println("VideoDecoder JPEG");
  if (!VideoDecoder.begin()) {
    Serial.println("video engine not ready");
    return;
  }
  VideoFrame frame;
  unsigned long t0 = millis();
  int ret = VideoDecoder.decodeJPEG(test_jpeg, test_jpeg_size, frame, VideoFrame::NV12);
  Serial.printf("decode: %d, %ux%u in %lu ms\n", ret, frame.width, frame.height, millis() - t0);
  if (ret != 0) return;
  Serial.printf("show: %d\n", VideoDecoder.show(frame, true));
  // the frame stays claimed while it is on the screen: keep it
}

void loop() {
  delay(1000);
}
