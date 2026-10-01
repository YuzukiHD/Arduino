// I2S0 with its internal loopback: sends a counting pattern and checks what comes back.
#include <I2S.h>

const int RATE = 48000;

void setup() {
  Serial.begin(115200);
  I2S.setLoopback(true);
  if (!I2S.begin(I2S_PHILIPS_MODE, RATE, 16)) {
    Serial.println("I2S.begin failed");
    while (true) delay(1000);
  }
  Serial.println("I2S0 loopback test");
}

void loop() {
  static uint16_t counter = 1;
  static long expect = -1;
  static uint32_t frames, errors;
  static unsigned long last;

  // keep the transmitter fed: left = counter, right = ~counter
  int16_t pair[2 * 64];
  if (I2S.availableForWrite() >= (int)sizeof(pair)) {
    for (int i = 0; i < 64; i++) {
      pair[2 * i] = (int16_t)counter;
      pair[2 * i + 1] = (int16_t)~counter;
      counter++;
    }
    I2S.write((const void *)pair, sizeof(pair));
  }

  // check what arrives, the first frames after the start may be silent
  while (I2S.available() >= 4) {
    uint16_t l = (uint16_t)I2S.readSample();
    uint16_t r = (uint16_t)I2S.readSample();
    if (expect < 0) {
      if (r == (uint16_t)~l && l != 0) expect = l; else continue;
    }
    if (l != (uint16_t)expect || r != (uint16_t)~expect) {
      errors++;
      if (r == (uint16_t)~l) expect = l;
    }
    expect = (expect + 1) & 0xffff;
    frames++;
  }

  if (millis() - last >= 2000) {
    last = millis();
    Serial.printf("%lu frames compared, %lu mismatches -> %s\n", (unsigned long)frames,
                  (unsigned long)errors, frames && !errors ? "PASS" : "FAIL");
  }
}
