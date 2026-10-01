// Shows a generated NV12 picture on the video plane and a text overlay on top of it.
#include <Display.h>

const int W = 640, H = 360;

void setup() {
  Serial.begin(115200);
  if (!Display.begin()) {
    Serial.println("display not ready");
    return;
  }
  uint8_t *y = (uint8_t *)aligned_alloc(64, W * H * 3 / 2);
  uint8_t *uv = y + W * H;
  for (int j = 0; j < H; j++)
    for (int i = 0; i < W; i++) y[j * W + i] = 16 + (i * 219) / W;
  for (int j = 0; j < H / 2; j++)
    for (int i = 0; i < W / 2; i++) {
      uv[j * W + 2 * i] = 64 + (i * 128) / (W / 2);       // Cb
      uv[j * W + 2 * i + 1] = 64 + (j * 128) / (H / 2);   // Cr
    }
  // the display reads the picture from memory: write it back from the data cache first
  Display.cacheFlush(y, W * H * 3 / 2);

  Display.fillScreen(0);  // alpha 0: transparent
  Display.setTextSize(4);
  Display.setTextColor(Display.color(255, 255, 255));
  Display.setCursor(40, 40);
  Display.print("overlay text");
  Display.show();
  Serial.println(Display.showYuv(y, uv, W, H, W, W) ? "yuv shown" : "yuv failed");
}

void loop() {
  delay(1000);
}
