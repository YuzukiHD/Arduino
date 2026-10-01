// Lists the tracks of /SD:/video540p.mp4 and walks the packets of both tracks.
#include <MP4.h>

MP4File file;

void setup() {
  Serial.begin(115200);
  if (!file.open("/SD:/video540p.mp4")) {
    Serial.println("cannot open the file (SD card in? mp4 with H.264/AAC?)");
    return;
  }
  Serial.printf("video: %s %ux%u, %lu frames, %lu ms\n", file.hasVideo() ? "H.264" : "none",
                file.width(), file.height(), (unsigned long)file.videoFrames(),
                (unsigned long)file.videoDurationMs());
  Serial.printf("audio: %s %lu Hz, %u ch, %lu packets, %lu ms\n", file.hasAudio() ? "AAC" : "none",
                (unsigned long)file.audioSampleRate(), file.audioChannels(),
                (unsigned long)file.audioFrames(), (unsigned long)file.audioDurationMs());

  MP4Packet p;
  uint32_t n = 0, keys = 0;
  uint64_t bytes = 0;
  while (file.nextVideo(p)) {
    n++;
    keys += p.key;
    bytes += p.size;
    if (n <= 3) Serial.printf("  video #%lu: size %lu, pts %lld us%s\n", (unsigned long)p.index,
                              (unsigned long)p.size, (long long)p.ptsUs, p.key ? ", key" : "");
  }
  Serial.printf("video: %lu packets, %lu key frames, %lu bytes\n", (unsigned long)n,
                (unsigned long)keys, (unsigned long)bytes);
  n = 0;
  while (file.nextAudio(p)) n++;
  Serial.printf("audio: %lu packets\n", (unsigned long)n);
  file.close();
}

void loop() {
  delay(1000);
}
