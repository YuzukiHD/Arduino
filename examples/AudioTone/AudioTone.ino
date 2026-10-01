// Plays a rising scale on the headphone / speaker output of the on-chip codec.
#include <Audio.h>

const uint16_t notes[] = {262, 294, 330, 349, 392, 440, 494, 523};

void setup() {
  Serial.begin(115200);
  if (!Audio.begin(48000)) {
    Serial.println("Audio.begin failed");
    while (true) delay(1000);
  }
  Audio.setVolume(70);
  Serial.println("Audio: playing a scale");
}

void loop() {
  for (unsigned i = 0; i < sizeof(notes) / sizeof(notes[0]); i++) {
    Serial.printf("%u Hz\n", notes[i]);
    Audio.playTone(notes[i], 300);
  }
  Audio.playSilence(500);
}
