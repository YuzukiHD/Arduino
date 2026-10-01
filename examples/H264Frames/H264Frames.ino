// Feeds an Annex B H.264 stream (from a buffer) to the decoder and counts the frames.
// Replace `stream`/`stream_size` with your data.
#include <VideoDecoder.h>

extern const uint8_t *stream;
extern size_t stream_size;
const uint8_t *stream = nullptr;
size_t stream_size = 0;

void setup() {
  Serial.begin(115200);
  if (!VideoDecoder.begin() || !VideoDecoder.openH264()) {
    Serial.println("cannot open the decoder");
    return;
  }
  size_t pos = 0;
  int frames = 0;
  bool flushed = false;
  while (true) {
    VideoFrame f;
    int r = VideoDecoder.getFrame(f);
    if (r == VideoDecoderClass::OK) {
      frames++;
      VideoDecoder.release(f);
    } else if (r == VideoDecoderClass::NODATA) {
      break;
    } else if (r == VideoDecoderClass::AGAIN) {
      if (pos < stream_size) {
        size_t used = 0;
        VideoDecoder.feed(stream + pos, stream_size - pos, -1, &used);
        pos += used;
      } else if (!flushed) {
        VideoDecoder.flush();
        flushed = true;
      } else {
        break;
      }
    } else if (r != VideoDecoderClass::BUSY) {
      break;
    }
  }
  VideoDecoder.closeStream();
  Serial.printf("%d frames\n", frames);
}

void loop() {
  delay(1000);
}
