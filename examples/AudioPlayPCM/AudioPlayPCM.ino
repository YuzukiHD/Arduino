// Plays a generated stereo sweep with Audio.writeSamples(): left rises, right falls.
// The second part shows the non-blocking form.
#include <Audio.h>

const int RATE = 48000;
const int FRAMES = 4800;           // 100 ms
int16_t buf[FRAMES * 2];

static void fill(double f0, double f1, double *phaseL, double *phaseR) {
  for (int i = 0; i < FRAMES; i++) {
    double t = (double)i / FRAMES;
    double fl = f0 + (f1 - f0) * t, fr = f1 + (f0 - f1) * t;
    *phaseL += 2 * PI * fl / RATE;
    *phaseR += 2 * PI * fr / RATE;
    buf[2 * i] = (int16_t)(8000 * sin(*phaseL));
    buf[2 * i + 1] = (int16_t)(8000 * sin(*phaseR));
  }
}

void setup() {
  Serial.begin(115200);
  Audio.begin(RATE, 2);
  Audio.setVolume(60);
}

void loop() {
  double pl = 0, pr = 0;
  Serial.println("blocking sweep");
  for (int step = 0; step < 10; step++) {
    fill(300 + step * 100, 400 + step * 100, &pl, &pr);
    Audio.writeSamples(buf, FRAMES * 2);
  }

  Serial.println("non-blocking sweep");
  int step = 0, offset = 0;
  bool have = false;
  unsigned long t0 = millis(), spins = 0;
  while (step < 10) {
    if (!have) { fill(1000 - step * 80, 1100 - step * 80, &pl, &pr); have = true; offset = 0; }
    offset += Audio.writeSamples(buf + offset, FRAMES * 2 - offset, false);
    if (offset >= FRAMES * 2) { have = false; step++; }
    spins++;                      // the sketch is free to do other work here
  }
  Serial.printf("queued in %lu ms, %lu loop passes\n", millis() - t0, spins);
  Audio.flush();
  Audio.playSilence(300);
}
