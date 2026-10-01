// Plays /SD:/video540p.mp4 (H.264 + AAC 48 kHz) in a loop and reports the
// statistics.
#include <MP4Player.h>

void setup() {
  Serial.begin(115200);
  if (!MP4Player.begin()) {
    Serial.print("begin failed: ");
    Serial.println(MP4Player.error());
    return;
  }
  MP4Player.setVolume(200);
}

void loop() {
  if (!MP4Player.start("/SD:/video540p.mp4")) {
    Serial.print("start failed: ");
    Serial.println(MP4Player.error());
    delay(2000);
    return;
  }
  Serial.printf("playing %ux%u, %lu frames, %lu ms\n", MP4Player.width(),
                MP4Player.height(), (unsigned long)MP4Player.frames(),
                (unsigned long)MP4Player.durationMs());
  unsigned long last = millis();
  while (MP4Player.update()) {
    if (millis() - last >= 5000) {
      last = millis();
      Serial.printf(
          "t=%lu ms shown=%lu dropped=%lu offset=%d ms underruns=%lu\n",
          (unsigned long)MP4Player.positionMs(),
          (unsigned long)MP4Player.framesShown(),
          (unsigned long)MP4Player.framesDropped(), MP4Player.avOffsetMs(),
          (unsigned long)MP4Player.audioUnderruns());
    }
  }
  Serial.printf("done: %lu shown, %lu dropped, %lu underruns %s\n",
                (unsigned long)MP4Player.framesShown(),
                (unsigned long)MP4Player.framesDropped(),
                (unsigned long)MP4Player.audioUnderruns(), MP4Player.error());
}
