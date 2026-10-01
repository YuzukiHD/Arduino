// Decodes the AAC track of /SD:/video540p.mp4 and prints the level of each second of sound.
// Depends on the MP4 library for the container.
#include <MP4.h>
#include <AACDecoder.h>

MP4File file;
AACDecoder dec;

void setup() {
  Serial.begin(115200);
  if (!file.open("/SD:/video540p.mp4") || !file.hasAudio()) {
    Serial.println("no file or no audio track");
    return;
  }
  size_t n = 0;
  const uint8_t *asc = file.audioExtra(&n);
  if (!dec.begin(asc, n)) {
    Serial.println("decoder init failed");
    return;
  }
  Serial.printf("AAC %lu Hz, %d channels\n", (unsigned long)dec.sampleRate(), dec.channels());

  uint8_t *buf = (uint8_t *)malloc(file.maxAudioPacket());
  static int16_t pcm[AACDecoder::MAX_PCM_SAMPLES];
  MP4Packet p;
  uint32_t frames = 0, perSecond = file.audioSampleRate(), sec = 0;
  int peak = 0;
  unsigned long t0 = millis();
  while (file.nextAudio(p)) {
    if (file.readAudio(p, buf) != 0) break;
    int got = dec.decode(buf, p.size, pcm);
    if (got < 0) continue;
    for (int i = 0; i < got * 2; i++) peak = max(peak, abs((int)pcm[i]));
    frames += got;
    if (frames >= perSecond) {
      frames -= perSecond;
      Serial.printf("second %lu: peak %d\n", (unsigned long)++sec, peak);
      peak = 0;
      if (sec >= 10) break;
    }
  }
  Serial.printf("decoded %lu s of sound in %lu ms\n", (unsigned long)sec, millis() - t0);
  free(buf);
}

void loop() {
  delay(1000);
}
