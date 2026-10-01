// G2D on the Display drawing buffer: fill, scaled/rotated blit, alpha blend, with timing.
#include <Display.h>
#include <G2D.h>

void setup() {
  Serial.begin(115200);
  if (!Display.begin() || !G2D.begin()) {
    Serial.println("display or G2D not ready");
    return;
  }
  G2DSurface screen = G2DSurface::fromDisplay(Display);
  int W = Display.width(), H = Display.height();

  unsigned long t0 = micros();
  int r = G2D.fill(screen, {0, 0, (uint16_t)W, (uint16_t)H}, 0xFF102030);
  Serial.printf("fill full screen: %d, %lu us\n", r, micros() - t0);

  // a 64x64 test tile in RGB565 made by the CPU
  static uint16_t tile[64 * 64] __attribute__((aligned(64)));
  for (int y = 0; y < 64; y++)
    for (int x = 0; x < 64; x++)
      tile[y * 64 + x] = ((x * 31 / 63) << 11) | ((y * 63 / 63) << 5) | 16;
  Display.cacheFlush(tile, sizeof(tile));
  G2DSurface t = G2DSurface::of(G2DFormat::RGB565, tile, 64, 64);

  r = G2D.blit(t, {0, 0, 64, 64}, screen, {20, 20, 256, 256});                         // scale x4, convert
  Serial.printf("scaled blit: %d\n", r);
  // rotate and flip are pure moves: same format as the destination, no scaling
  static uint32_t tile32[64 * 64] __attribute__((aligned(64)));
  for (int i = 0; i < 64 * 64; i++) {
    int x = i % 64, y = i / 64;
    tile32[i] = 0xFF000000 | ((x * 255 / 63) << 16) | ((y * 255 / 63) << 8) | 0x40;
  }
  Display.cacheFlush(tile32, sizeof(tile32));
  G2DSurface t32 = G2DSurface::of(G2DFormat::ARGB8888, tile32, 64, 64);
  r = G2D.blit(t32, {0, 0, 64, 64}, screen, {300, 20, 64, 64}, G2DRotation::Rot90);
  Serial.printf("rotate 90: %d\n", r);
  r = G2D.blit(t32, {0, 0, 64, 64}, screen, {460, 20, 64, 64}, G2DRotation::Rot0, G2DClass::FlipH);
  Serial.printf("flip: %d\n", r);

  // translucent red rectangle over the tiles
  G2D.fill(screen, {100, 100, 300, 120}, 0x80FF0000);                                      // plain fill keeps alpha
  static uint32_t overlay[200 * 100] __attribute__((aligned(64)));
  for (int i = 0; i < 200 * 100; i++) overlay[i] = 0xA000FF00;
  Display.cacheFlush(overlay, sizeof(overlay));
  G2DSurface ov = G2DSurface::of(G2DFormat::ARGB8888, overlay, 200, 100);
  r = G2D.blend(ov, {0, 0, 200, 100}, screen, {200, 150, 200, 100}, screen, {200, 150, 200, 100});
  Serial.printf("blend: %d\n", r);

  Display.setTextSize(2);
  Display.setTextColor(0xFFFFFFFF);
  Display.setCursor(20, 320);
  Display.println("G2D fill / blit / rotate / flip / blend");
  Display.show();
}

void loop() {
  delay(1000);
}
