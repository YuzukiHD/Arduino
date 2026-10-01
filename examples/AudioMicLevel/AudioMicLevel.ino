// Prints the microphone level every 200 ms (a bar and the peak value).
#include <Audio.h>

void setup() {
  Serial.begin(115200);
  if (!Audio.beginRecord(48000)) {
    Serial.println("Audio.beginRecord failed");
    while (true) delay(1000);
  }
  Audio.setInputGain(80);
  Serial.println("Audio: microphone level (speak or clap)");
}

void loop() {
  int16_t chunk[960];                         // 20 ms at 48 kHz
  size_t n = Audio.readSamples(chunk, 960, 500);
  static unsigned long last;
  if (millis() - last >= 200) {
    last = millis();
    int pct = Audio.levelPercent();
    char bar[41];
    int len = pct * 40 / 100;
    for (int i = 0; i < 40; i++) bar[i] = i < len ? '#' : '.';
    bar[40] = 0;
    Serial.printf("%3d%% peak %5d [%s] (%u samples)\n", pct, Audio.level(), bar, (unsigned)n);
  }
}
