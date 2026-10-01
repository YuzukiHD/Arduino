// Talks to a 4-wire SPI panel (ST7789-style 240x320) on PD0..PD5: reset, init, colour bars.
// Without a panel the calls still complete (the bus has no readback), so this only proves the host.
#include <DBI.h>

void setup() {
  Serial.begin(115200);
  Serial.println(DBI.begin() ? "DBI host ready" : "DBI host not ready");
  Serial.printf("reset: %d\n", DBI.reset());
  Serial.printf("sleep out: %d\n", DBI.sleepOut());
  const uint8_t madctl[] = {0x00};
  const uint8_t colmod[] = {0x55};  // 16 bit pixels
  Serial.printf("madctl: %d\n", DBI.command(0x36, madctl));
  Serial.printf("colmod: %d\n", DBI.command(0x3A, colmod));
  Serial.printf("display on: %d\n", DBI.displayOn());

  static uint16_t row[240];
  const uint16_t bars[] = {0xF800, 0x07E0, 0x001F, 0xFFFF};
  for (int y = 0; y < 320; y++) {
    for (int x = 0; x < 240; x++) row[x] = bars[(y * 4) / 320];
    int r = DBI.writeRGB565(0, y, 240, 1, row);
    if (r) { Serial.printf("row %d failed: %d\n", y, r); break; }
  }
  Serial.println("bars sent");
}

void loop() {
  delay(1000);
}
