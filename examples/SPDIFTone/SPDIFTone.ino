// S/PDIF transmitter on PE6: a 1 kHz tone at 48 kHz, 16 bit stereo.
#include <SPDIF.h>

void setup() {
  Serial.begin(115200);
  if (!SPDIF.begin(48000, 16)) {
    Serial.println("SPDIF.begin failed");
    while (true) delay(1000);
  }
  Serial.println("S/PDIF tone on PE6");
}

void loop() {
  static double phase;
  int16_t buf[2 * 96];                // 2 ms
  for (int i = 0; i < 96; i++) {
    int16_t v = (int16_t)(10000 * sin(phase));
    buf[2 * i] = buf[2 * i + 1] = v;
    phase += 2 * PI * 1000.0 / 48000.0;
    if (phase > 2 * PI) phase -= 2 * PI;
  }
  SPDIF.write((const uint8_t *)buf, sizeof(buf));
}
